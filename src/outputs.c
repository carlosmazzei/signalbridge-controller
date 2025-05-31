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
output_drivers_t output_drivers; // Global variable to hold output drivers

/* Function declarations */

static int8_t init_mux(void)
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

void init_driver(void)
{
	// Check the config map and initialize the drivers
	for (uint8_t i = 0; i < MAX_SPI_INTERFACES; i++)
	{
		output_drivers.driver_handles[i] = NULL;

		if (DEVICE_TM1639_DIGIT == device_config_map[i] ||
		    DEVICE_TM1639_LED == device_config_map[i])
		{
			// Initialize TM1639 driver
			output_drivers.driver_handles[i] = tm1639_init(i, select_interface, spi0, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN);
			if (!output_drivers.driver_handles[i])
			{
				// @todo: Handle error
				continue;
			}
		}
		else if (DEVICE_GENERIC_DIGIT == device_config_map[i] ||
		         DEVICE_GENERIC_LED == device_config_map[i])
		{
			// Initialize generic driver (if needed)
			/* @todo: initialize generic devices with generic drivers */
		}
		else
		{
			// Unsupported device type, handle error if needed
			continue;
		}
	}
}

bool output_init(void)
{
	// Enable Pico default LED pin (GPIO 25)
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
	gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

	// Create mutex
	if (!spi_mutex)
	{
		spi_mutex = xSemaphoreCreateMutex();
		// Error returning mutex
		if (!spi_mutex)
		{
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

	init_driver();

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

	/* Convert controller_id to physical CS (0-based indexing) */
	uint8_t physical_cs = controller_id - 1;

	/* Take SPI mutex */
	if (pdTRUE != xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)))
		return -1;

	/* Process BCD data if we have enough bytes */
	if (length >= 4)  // Need 4 bytes for 8 digits (2 digits per byte)
	{
		/* Convert packed BCD payload to digits array for display */
		uint8_t digits[8];
		digits[0] = (payload[1] >> 4) & 0x0F;
		digits[1] = payload[1] & 0x0F;
		digits[2] = (payload[2] >> 4) & 0x0F;
		digits[3] = payload[2] & 0x0F;
		digits[4] = (payload[3] >> 4) & 0x0F;
		digits[5] = payload[3] & 0x0F;
		digits[6] = (payload[4] >> 4) & 0x0F;
		digits[7] = payload[4] & 0x0F;

		/* Check if this is a TM1639 digit display device */
		if (DEVICE_TM1639_DIGIT == device_config_map[physical_cs] ||
		    DEVICE_GENERIC_DIGIT == device_config_map[physical_cs])
		{
			/* Display the digits using TM1639 library */
			const output_driver_t *handle = output_drivers.driver_handles[physical_cs];
			if (handle != NULL && handle->set_digits)
			{
				handle->set_digits(digits, sizeof(digits));
			}
		}

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

int led_out(const uint8_t *payload, uint8_t length)
{
	/* Check if controller ID is valid (1-based indexing in the protocol) */
	uint8_t controller_id = payload[0];
	if (DEVICE_GENERIC_LED != device_config_map[controller_id] &&
	    (DEVICE_TM1639_LED != device_config_map[controller_id]))
	{
		return -1;
	}

	/* Get physical CS directly from lookup table (adjust for 0-based indexing) */
	uint8_t index = payload[1];
	uint8_t ledstate = payload[2];
	uint8_t physical_cs = controller_id - 1;

	/* Take SPI mutex */
	if (pdTRUE != xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)))
		return -1;


	/* Select the chip */
	select_interface(physical_cs, true);

	/* Set the LED states for the specified row/column */
	/* Check if this is a TM1639 digit display device */
	if (DEVICE_TM1639_LED == device_config_map[physical_cs] ||
	    DEVICE_GENERIC_LED == device_config_map[physical_cs])
	{
		// Send to TM1639 driver all the LED states
	}

	/* Release SPI mutex */
	xSemaphoreGive(spi_mutex);

}

void set_pwm_duty(uint8_t duty)
{
	// Square the fade value to make the LED's brightness appear more linear
	// Note this range matches with the wrap value
	pwm_set_gpio_level(PWM_PIN, duty * duty);
}