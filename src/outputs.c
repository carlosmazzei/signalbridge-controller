/**
 * @file outputs.c
 * @brief Functions to output Leds, PWM and other
 * @author Carlos Mazzei
 *
 * This file contains the functions to output Leds, PWM and 7 segment displays
 *
 * @copyright (c) 2020-2024 Carlos Mazzei
 * All rights reserved.
 */

#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "tm1639.h"
#include "outputs.h"

/**
 * Maximum size of the array that holds the LED states.
 */
#define MAX_LED_STATE_SIZE 8

/**
 * LED states.
 */
uint8_t led_states[MAX_LED_STATE_SIZE];

/* Global variables */
static const uint8_t device_config_map[MAX_SPI_INTERFACES] = DEVICE_CONFIG;
static SemaphoreHandle_t spi_mutex = NULL;

/**
 * @brief Initialize the multiplexer for chip select control
 *
 * @return int Error code, 0 if successful
 */
static int8_t init_mux()
{
	// Initialize multiplexer pins
	gpio_init(MUX_ENABLE);
	gpio_init(MUX_A_PIN);
	gpio_init(MUX_B_PIN);
	gpio_init(MUX_C_PIN);

	// Check for errors in GPIO initialization
	if (gpio_get_function(MUX_A_PIN) != GPIO_FUNC_SIO ||
	    gpio_get_function(MUX_B_PIN) != GPIO_FUNC_SIO ||
	    gpio_get_function(MUX_C_PIN) != GPIO_FUNC_SIO ||
	    gpio_get_function(MUX_ENABLE) != GPIO_FUNC_SIO)
	{
		return TM1639_ERR_GPIO_INIT;
	}

	gpio_set_dir(MUX_A_PIN, GPIO_OUT);
	gpio_set_dir(MUX_B_PIN, GPIO_OUT);
	gpio_set_dir(MUX_C_PIN, GPIO_OUT);
	gpio_set_dir(MUX_ENABLE, GPIO_OUT);

	// Default state (all high, no chip selected)
	gpio_put(MUX_ENABLE, 0);
	gpio_put(MUX_A_PIN, 1);
	gpio_put(MUX_B_PIN, 1);
	gpio_put(MUX_C_PIN, 1);

	return TM1639_OK;
}

/**
 * @brief Select the interace chip through multiplexer
 *
 * @param chip_select Chip select number (0-7)
 * @param select True to select (STB low), false to deselect (STB high)
 */
static void select_interface(uint8_t chip_select, bool select)
{
	if (chip_select >= MAX_SPI_INTERFACES)
	{
		return;
	}

	if (select)
	{
		// Convert chip number to individual bits for multiplexer control
		gpio_put(MUX_A_PIN, (chip_select & 0x01));             // LSB
		gpio_put(MUX_B_PIN, (chip_select & 0x02) >> 1);        // middle bit
		gpio_put(MUX_C_PIN, (chip_select & 0x04) >> 2);        // MSB

		gpio_put(MUX_ENABLE, 1);
	}
	else
	{
		// Set multiplexer to no output (all selector pins high)
		gpio_put(MUX_ENABLE, 0);
	}

	// Small delay to ensure stable signal
	sleep_us(1);
}

/** @brief Initialize the outputs.
 *
 * Initialize all the outputs needed for the application: SPI, LEDs, PWM.
 *
 * @return True if successful.
 *
 * @todo Implement the I2C initialization.
 *
 */
bool output_init(void)
{
	// Enable Pico default LED pin (GPIO 25)
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
	gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

	// Create mutex
	if (!spi_mutex) {
		spi_mutex = xSemaphoreCreateMutex();
		// Error returning mutex
		if (!spi_mutex) {
			return false;
		}
	}

	// Initialize LED outputs (using TM1639)
	init_mux();

	spi_init(spi0, SPI_FREQUENCY);
	// Set SPI format (8 bits, CPOL=0, CPHA=0, LSB first)
	spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);

	gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

	// Verify SPI pin configuration
	if (gpio_get_function(PICO_DEFAULT_SPI_SCK_PIN) != GPIO_FUNC_SPI ||
	    gpio_get_function(PICO_DEFAULT_SPI_TX_PIN) != GPIO_FUNC_SPI)
	{
		return false;
	}

	// Configure half-duplex (we'll handle direction changes when reading keys)
	// No need to initialize MISO/RX pin in hardware SPI mode yet

	// Make the SPI pins available to picotool
	bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI))

	// Configure PWM pin
	gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
	pwm_config config = pwm_get_default_config();
	pwm_config_set_clkdiv(&config, 10.f);
	pwm_init(slice_num, &config, true);

	return true;
}

int display_out(const uint8_t *payload, uint8_t length)
{
	/* Check if controller_id is of type digit */
	uint8_t controller_id = payload[0];
	if (DEVICE_GENERIC_DIGIT != device_config_map[controller_id] &&
	    (DEVICE_TM1639_DIGIT != device_config_map[controller_id]))
	{
		return -1;
	}

	/* Take SPI mutex */
	if (pdTRUE != xSemaphoreTake(spi_mutex, portMAX_DELAY))
	{
		return -1;
	}

	/* Process BCD data if we have enough bytes */
	if (length >= 4)  // Need 4 bytes for 8 digits (2 digits per byte)
	{
		/* Convert controller_id to physical CS (0-based indexing) */
		uint8_t physical_cs = controller_id - 1;

		/* Select the chip */
		select_interface(physical_cs, true);

		/* Convert packed BCD payload to digits array for display */
		uint8_t digits[8];
		digits[0] = (payload[1] >> 4) & 0x0F;
		digits[1] = payload[1] & 0x0F;
		digits[2] = (payload[2] >> 4) & 0x0F;
		digits[3] = payload[2] & 0x0F;
		digits[4] = (payload[3] >> 4) & 0x0F;
		digits[5] = bcdpayload[3] & 0x0F;
		digits[6] = (payload[4] >> 4) & 0x0F;
		digits[7] = payload[4] & 0x0F;

		/* Check if this is a TM1639 digit display device */
		if (DEVICE_TM1639_DIGIT == device_config_map[physical_cs])
		{
			/* Display the digits using TM1639 library */
			tm1639_display_digits(digits, 8);
		}
		else if (DEVICE_GENERIC_DIGIT == device_config_map[physical_cs])
		{
			/* Display the digits using generic library */
			/* @todo: implement generic_display_digits(digits, 8); */
		}

		/* Disable chip select */
		select_interface(physical_cs, false);

		/* Release SPI mutex */
		xSemaphoreGive(spi_mutex);

		return 0;
	}
	else
	{
		/* Disable chip select and release mutex before returning error */
		select_interface(physical_cs, false);
		xSemaphoreGive(spi_mutex);
		return -1;
	}


}

static void prvProcessLEDCommand(uint8_t controller_id, uint8_t index, uint8_t ledstate)
{
	/* Check if controller ID is valid (1-based indexing in the protocol) */
	if (0 == controller_id || controller_id > NUM_LED_CONTROLLERS)
	{
		//printf("Error: Invalid LED controller ID %d\n", controller_id);
		return;
	}

	/* Check if index is valid */
	if (index >= 8)
	{
		//printf("Error: Invalid LED index %d\n", index);
		return;
	}

	/* Get physical CS directly from lookup table (adjust for 0-based indexing) */
	uint8_t physical_cs = led_controller_to_cs[controller_id - 1];

	/* Take SPI mutex */
	if (pdTRUE == xSemaphoreTake(spi_mutex, portMAX_DELAY))
	{
		/* Select the chip */
		select_interface(physical_cs, true);

		/* Set the LED states for the specified row/column */
		tm1639_set_led_matrix_row(index, ledstate);

		/* Disable chip select */
		select_interface(physical_cs, false);

		/* Release SPI mutex */
		xSemaphoreGive(spi_mutex);
	}
}

/** @brief Output the state of the LEDs to the SPI bus.
 *
 * @param index The index of the first byte of LED states to output.
 * @param states The LED states to output.
 * @param len The number of LED states to output.
 *
 * @return True if successful.
 */
bool led_out(uint8_t index, const uint8_t *states, uint8_t len)
{
	if (index + len < MAX_LED_STATE_SIZE + 1)
	{
		for (int i = 0; i < len; i++)
		{
			led_states[index + i] = states[i];
		}

		int count = spi_write_blocking(spi_default, led_states, sizeof(led_states));
		led_select();
		if (count > 0) return true;
	}
	return false;
}

void set_pwm_duty(uint8_t duty)
{
	// Square the fade value to make the LED's brightness appear more linear
	// Note this range matches with the wrap value
	pwm_set_gpio_level(PWM_PIN, duty * duty);
}