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

/* Global variables */
static const uint8_t device_config_map[MAX_SPI_INTERFACES] = DEVICE_CONFIG;
static SemaphoreHandle_t spi_mutex = NULL;
output_drivers_t output_drivers; // Global variable to hold output drivers
out_statistics_counters_t out_statistics_counters;

/* Function declarations */

/**
 * @brief Initialize the multiplexer for chip select control
 *
 * @return int Error code, 0 if successful
 */
static uint8_t init_mux(void)
{
	uint8_t result = TM1639_OK;

	// Initialize multiplexer pins
	gpio_init(MUX_ENABLE);
	gpio_init(MUX_A_PIN);
	gpio_init(MUX_B_PIN);
	gpio_init(MUX_C_PIN);

	// Check for errors in GPIO initialization
	if ((gpio_get_function(MUX_A_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(MUX_B_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(MUX_C_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(MUX_ENABLE) != GPIO_FUNC_SIO))
	{
		result = TM1639_ERR_GPIO_INIT;
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

	return result;
}

/**
 * @brief Select the interace chip through multiplexer
 *
 * @param chip_select Chip select number (0-7)
 * @param select True to select (STB low), false to deselect (STB high)
 * @return uint8_t Result code, OUTPUT_OK if successful, otherwise an error code
 */
static uint8_t select_interface(uint8_t chip_select, bool select)
{
	uint8_t result = OUTPUT_OK;

	// Check if chip_select is within valid range
	if (chip_select >= (uint8_t)MAX_SPI_INTERFACES)
	{
		result = OUTPUT_ERR_INVALID_PARAM;
	}

	if (select)
	{
		// Convert chip number to individual bits for multiplexer control
		gpio_put(MUX_A_PIN, (chip_select & (uint8_t)0x01));       // LSB
		gpio_put(MUX_B_PIN, (chip_select & (uint8_t)0x02) >> 1);  // middle bit
		gpio_put(MUX_C_PIN, (chip_select & (uint8_t)0x04) >> 2);  // MSB

		gpio_put(MUX_ENABLE, 1);
	}
	else
	{
		// Set multiplexer to no output (all selector pins high)
		gpio_put(MUX_ENABLE, 0);
	}

	// Small delay to ensure stable signal
	sleep_us(1);

	return result;
}

/**
 * @brief Initialize the output drivers
 * This function initializes the output drivers based on the device configuration map.
 * It checks the device type for each SPI interface and initializes the corresponding driver.
 * For TM1639 devices, it initializes the TM1639 driver with the appropriate parameters.
 * For generic devices, it can be extended to initialize a generic driver if needed.
 * If a device type is not supported, it skips initialization for that interface.
 */
static uint8_t init_driver(void)
{
	uint8_t result = OUTPUT_OK;
	// Check the config map and initialize the drivers
	for (uint8_t i = 0; i < (uint8_t)MAX_SPI_INTERFACES; i++)
	{
		output_drivers.driver_handles[i] = NULL;

		if (((uint8_t)DEVICE_TM1639_DIGIT == device_config_map[i]) ||
		    ((uint8_t)DEVICE_TM1639_LED == device_config_map[i]))
		{
			// Initialize TM1639 driver
			output_drivers.driver_handles[i] = tm1639_init(i,
			                                               &select_interface,
			                                               spi0,
			                                               PICO_DEFAULT_SPI_TX_PIN,
			                                               PICO_DEFAULT_SPI_SCK_PIN);
			if (!output_drivers.driver_handles[i])
			{
				result = OUTPUT_ERR_INIT;
				out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
				continue;
			}
		}
		else if (((uint8_t)DEVICE_GENERIC_DIGIT == device_config_map[i]) ||
		         ((uint8_t)DEVICE_GENERIC_LED == device_config_map[i]))
		{
			// Initialize generic driver (if needed)
			/* @todo: initialize generic devices with generic drivers */
		}
		else
		{
			result = OUTPUT_ERR_INIT;
			out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
			continue;
		}
	}

	return result;
}

uint8_t output_init(void)
{
	uint8_t result = OUTPUT_OK;

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
			result = OUTPUT_ERR_INIT;
		}
	}

	// Initialize LED outputs (using TM1639)
	uint8_t result_mux = init_mux();
	if (result_mux != (uint8_t)TM1639_OK)
	{
		out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
		result = OUTPUT_ERR_INIT;
	}

	spi_init(spi0, SPI_FREQUENCY);
	// Set SPI format (8 bits, CPOL=0, CPHA=0, LSB first)
	spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);

	gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

	// Verify SPI pin configuration
	if ((gpio_get_function(PICO_DEFAULT_SPI_SCK_PIN) != GPIO_FUNC_SPI) ||
	    (gpio_get_function(PICO_DEFAULT_SPI_TX_PIN) != GPIO_FUNC_SPI))
	{
		result =  OUTPUT_ERR_INIT;
	}

	// Configure half-duplex (we'll handle direction changes when reading keys)
	// No need to initialize MISO/RX pin in hardware SPI mode yet

	// Make the SPI pins available to picotool
	bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI)) // cppcheck-suppress unknownMacro

	// Configure PWM pin
	gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
	pwm_config config = pwm_get_default_config();
	pwm_config_set_clkdiv(&config, 10.f);
	pwm_init(slice_num, &config, true);

	uint8_t result_init = init_driver();
	if (result_init != (uint8_t)OUTPUT_OK)
	{
		out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
		result = OUTPUT_ERR_INIT;
	}

	return result;
}

uint8_t display_out(const uint8_t *payload, uint8_t length)
{
	/**
	 * The expected payload is controller id for the first byte
	 * 4 bytes using BCD for each digit in the lower and upper nibble
	 * Last byte as dot position
	 */
	uint8_t result = OUTPUT_OK;

	/* Load controller id (first byte of the payload) */
	uint8_t controller_id = payload[0];

	/* Convert controller_id to physical CS (0-based indexing) */
	uint8_t physical_cs = controller_id - (uint8_t)1;

	if (((uint8_t)DEVICE_GENERIC_DIGIT != device_config_map[physical_cs]) &&
	    ((uint8_t)DEVICE_TM1639_DIGIT != device_config_map[physical_cs]))
	{
		out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
		result = OUTPUT_ERR_DISPLAY_OUT;
	}

	/* Take SPI mutex */
	if (pdTRUE != xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)))
	{
		result = OUTPUT_ERR_SEMAPHORE;
	}

	/* Process BCD data if we have enough bytes */
	/* Need 4 bytes for 8 digits (2 digits per byte) and 1 for the dot position */
	if ((length >= (uint8_t)6) &&
	    ((uint8_t)OUTPUT_OK == result))
	{
		/* Convert packed BCD payload to digits array for display */
		uint8_t digits[8];
		digits[0] = (payload[1] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[1] = payload[1] & (uint8_t)0x0F;
		digits[2] = (payload[2] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[3] = payload[2] & (uint8_t)0x0F;
		digits[4] = (payload[3] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[5] = payload[3] & (uint8_t)0x0F;
		digits[6] = (payload[4] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[7] = payload[4] & (uint8_t)0x0F;

		/* Send digits to the corresponding driver */
		output_driver_t *handle = output_drivers.driver_handles[physical_cs];
		if ((handle != NULL) && (handle->set_digits))
		{
			handle->set_digits(handle, digits, sizeof(digits), payload[5]);
		}
	}
	else
	{
		/* Disable chip select and release mutex before returning error */
		if ((uint8_t)OUTPUT_OK != select_interface(physical_cs, false))
		{
			out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
		}
	}

	/* Release SPI mutex */
	if (pdFALSE == xSemaphoreGive(spi_mutex))
	{
		result = OUTPUT_ERR_SEMAPHORE;
	}

	return result;
}

uint8_t led_out(const uint8_t *payload, uint8_t length)
{
	/**
	 * The expected payload is controller id for the first byte,
	 * next byte is the index of the LED state to set,
	 * next byte is the LED state (0-255) for the specified row/column
	 */
	uint8_t result = OUTPUT_OK;

	/* Check if payload length is valid */
	if (length < (uint8_t)3)
	{
		out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
		result = OUTPUT_ERR_INVALID_PARAM;
	}

	/* Check if controller ID is valid (1-based indexing in the protocol) */
	uint8_t controller_id = payload[0];

	/* Convert controller_id to physical CS (0-based indexing) */
	uint8_t physical_cs = controller_id - (uint8_t)1;

	if (((uint8_t)DEVICE_GENERIC_LED != device_config_map[physical_cs]) &&
	    ((uint8_t)DEVICE_TM1639_LED != device_config_map[physical_cs]))
	{
		result = OUTPUT_ERR_INVALID_PARAM;
	}

	if ((uint8_t)OUTPUT_OK == result)
	{
		/* Get physical CS directly from lookup table (adjust for 0-based indexing) */
		uint8_t index = payload[1];
		uint8_t ledstate = payload[2];

		/* Take SPI mutex */
		if (pdTRUE == xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)))
		{
			/* Set the LED states for the specified row/column */
			/* Check if this is a TM1639 digit display device */
			if (((uint8_t)DEVICE_TM1639_LED == device_config_map[physical_cs]) ||
			    ((uint8_t)DEVICE_GENERIC_LED == device_config_map[physical_cs]))
			{
				/* Display the digits using TM1639 library */
				output_driver_t *handle = output_drivers.driver_handles[physical_cs];
				if ((handle != NULL) && (handle->set_digits))
				{
					handle->set_leds(handle, index, ledstate);
				}
			}
		}
		else
		{
			result = OUTPUT_ERR_SEMAPHORE;
		}

	}

	/* Release SPI mutex */
	if(pdTRUE != xSemaphoreGive(spi_mutex))
	{
		result = OUTPUT_ERR_SEMAPHORE;
	}

	return result;
}

void set_pwm_duty(uint8_t duty)
{
	// Square the fade value to make the LED's brightness appear more linear
	// Note this range matches with the wrap value
	pwm_set_gpio_level(PWM_PIN, duty * duty);
}