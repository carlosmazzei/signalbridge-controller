/**
 * @file outputs.c
 * @brief Implementation of output control functions: LEDs, PWM, and 7-segment displays.
 * @author
 *   - Carlos Mazzei
 * @date 2020-2025
 *
 * This file contains functions responsible for initializing and controlling output devices,
 * including LEDs, PWM, and 7-segment displays, using specific drivers (e.g., TM1639) and
 * SPI multiplexing. It also manages error statistics and synchronization for SPI bus access.
 *
 * @copyright
 * (c) 2020-2025 Carlos Mazzei. All rights reserved.
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
 * @brief Device configuration map for all SPI interfaces.
 *
 * This array holds the device type for each SPI interface, as defined by DEVICE_CONFIG.
 * Each entry corresponds to a controller ID (0-based).
 */
static const uint8_t device_config_map[MAX_SPI_INTERFACES] = DEVICE_CONFIG;

/**
 * @brief Mutex for synchronizing access to SPI operations.
 *
 * This semaphore ensures that only one task accesses the SPI bus at a time.
 */
static SemaphoreHandle_t spi_mutex = NULL;

/**
 * @brief Structure holding all output driver handles.
 *
 * This global variable stores pointers to the initialized output drivers for each interface.
 */
output_drivers_t output_drivers;

/**
 * @brief Output statistics counters.
 *
 * This global variable holds counters for various output-related errors and states.
 */
out_statistics_counters_t out_statistics_counters;

/* Function declarations */

/**
 * @brief Initialize the multiplexer for chip select control
 * This function initializes the GPIO pins used for the multiplexer
 *
 * @return output_result_t Error code, 0 if successful
 */
static output_result_t init_mux(void)
{
	tm1639_result_t result = TM1639_OK;

	// Initialize multiplexer pins
	gpio_init(SPI_MUX_CS);
	gpio_init(SPI_MUX_A_PIN);
	gpio_init(SPI_MUX_B_PIN);
	gpio_init(SPI_MUX_C_PIN);

	// Check for errors in GPIO initialization
	if ((gpio_get_function(SPI_MUX_A_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(SPI_MUX_B_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(SPI_MUX_C_PIN) != GPIO_FUNC_SIO) ||
	    (gpio_get_function(SPI_MUX_CS) != GPIO_FUNC_SIO))
	{
		result = TM1639_ERR_GPIO_INIT;
	}

	gpio_set_dir(SPI_MUX_A_PIN, GPIO_OUT);
	gpio_set_dir(SPI_MUX_B_PIN, GPIO_OUT);
	gpio_set_dir(SPI_MUX_C_PIN, GPIO_OUT);
	gpio_set_dir(SPI_MUX_CS, GPIO_OUT);

	// Default state (all high, no chip selected)
	gpio_put(SPI_MUX_CS, 0);
	gpio_put(SPI_MUX_A_PIN, 1);
	gpio_put(SPI_MUX_B_PIN, 1);
	gpio_put(SPI_MUX_C_PIN, 1);

	output_result_t output_result  = OUTPUT_OK;
	if (result != TM1639_OK)
	{
		out_statistics_counters.counters[OUT_INIT_ERROR]++;
		output_result = OUTPUT_ERR_INIT;
	}
	else
	{
		output_result = OUTPUT_OK;
	}

	return output_result;
}

/**
 * @brief Select the interace chip through multiplexer
 *  This function selects the specified chip by setting the multiplexer pins
 *
 * @param[in] chip_select Chip select number (0-7)
 * @param[in] select True to select (STB low), false to deselect (STB high)
 * @return output_result_t Result code, OUTPUT_OK if successful, otherwise an error code
 */
static output_result_t select_interface(uint8_t chip_select, bool select)
{
	output_result_t result = OUTPUT_OK;

	// Check if chip_select is within valid range
	if (chip_select >= (uint8_t)MAX_SPI_INTERFACES)
	{
		result = OUTPUT_ERR_INVALID_PARAM;
		out_statistics_counters.counters[OUT_INVALID_PARAM_ERROR]++;
	}

	if (select)
	{
		// Convert chip number to individual bits for multiplexer control
		gpio_put(SPI_MUX_A_PIN, (chip_select & (uint8_t)0x01));       // LSB
		gpio_put(SPI_MUX_B_PIN, (chip_select & (uint8_t)0x02) >> 1);  // middle bit
		gpio_put(SPI_MUX_C_PIN, (chip_select & (uint8_t)0x04) >> 2);  // MSB

		gpio_put(SPI_MUX_CS, 1);
	}
	else
	{
		// Set multiplexer to no output (all selector pins high)
		gpio_put(SPI_MUX_CS, 0);
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
 *
 * @return output_result_t Result code, OUTPUT_OK if successful, otherwise an error code
 * @note This function assumes that the device_config_map is correctly defined and matches the expected device types.
 *
 */
static output_result_t init_driver(void)
{
	output_result_t result = OUTPUT_OK;
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
			/** @todo: initialize generic devices with generic drivers */
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

output_result_t output_init(void)
{
	output_result_t result = OUTPUT_OK;

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

	// Initialize multiplexer
	output_result_t result_mux = init_mux();
	if (result_mux != OUTPUT_OK)
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

	output_result_t result_init = init_driver();
	if (result_init != OUTPUT_OK)
	{
		out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
		result = OUTPUT_ERR_INIT;
	}

	return result;
}

/**
 * @brief Sends a BCD-encoded digit payload to the appropriate output driver.
 *
 * The expected payload format is:
 * - Byte 0: Controller ID (1-based)
 * - Bytes 1-4: Packed BCD digits (2 digits per byte, lower and upper nibble)
 * - Byte 5: Dot position
 *
 * @param[in] payload Pointer to the payload buffer.
 * @param[in] length  Length of the payload buffer (should be at least 6).
 * @return OUTPUT_OK on success, or an error code on failure.
 */
output_result_t display_out(const uint8_t *payload, uint8_t length)
{
	output_result_t result = OUTPUT_OK;
	uint8_t physical_cs;

	/**
	 * @par Parameter validation
	 * Checks for:
	 * - Null pointer
	 * - Minimum payload length
	 * - Valid controller ID (1-based, must be within range)
	 */
	if ((NULL == payload) ||
	    (length < (uint8_t)6) ||
	    ((uint8_t)0 == payload[0]) ||
	    (payload[0] > (uint8_t)MAX_SPI_INTERFACES))
	{
		out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
		result = OUTPUT_ERR_INVALID_PARAM;
	}
	else
	{
		physical_cs = payload[0] - (uint8_t)1;

		/**
		 * @par Device type validation
		 * Checks if the device type is supported for display output.
		 */
		if (((uint8_t)DEVICE_GENERIC_DIGIT != device_config_map[physical_cs]) &&
		    ((uint8_t)DEVICE_TM1639_DIGIT != device_config_map[physical_cs]))
		{
			out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
			result = OUTPUT_ERR_INVALID_PARAM;
		}
	}

	/**
	 * @par Mutex acquisition
	 * Tries to take the SPI mutex if parameters are valid.
	 */
	if ((OUTPUT_OK == result) &&
	    (pdTRUE != xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000))))
	{
		result = OUTPUT_ERR_SEMAPHORE;
	}

	/**
	 * @par BCD processing and driver call
	 * Processes BCD data and sends it to the driver if all checks passed.
	 */
	if ((OUTPUT_OK == result) && (length >= (uint8_t)6))
	{
		uint8_t digits[8];
		digits[0] = (payload[1] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[1] = payload[1] & (uint8_t)0x0F;
		digits[2] = (payload[2] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[3] = payload[2] & (uint8_t)0x0F;
		digits[4] = (payload[3] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[5] = payload[3] & (uint8_t)0x0F;
		digits[6] = (payload[4] >> (uint8_t)4) & (uint8_t)0x0F;
		digits[7] = payload[4] & (uint8_t)0x0F;

		output_driver_t *handle = output_drivers.driver_handles[physical_cs];
		if ((handle != NULL) && (handle->set_digits))
		{
			handle->set_digits(handle, digits, sizeof(digits), payload[5]);
		}
		else
		{
			/* If the driver handle is invalid, try to deselect the chip and set error */
			(void)select_interface(physical_cs, false);
			out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
			result = OUTPUT_ERR_DISPLAY_OUT;
		}
	}

	/**
	 * @par Mutex release
	 * Releases the SPI mutex (if it was taken).
	 */
	if (pdFALSE == xSemaphoreGive(spi_mutex))
	{
		result = OUTPUT_ERR_SEMAPHORE;
	}

	return result;
}


output_result_t led_out(const uint8_t *payload, uint8_t length)
{
	output_result_t result = OUTPUT_OK;
	uint8_t physical_cs;

	/**
	 * @par Parameter validation
	 * Checks for:
	 * - Null pointer
	 * - Minimum payload length
	 * - Valid controller ID (1-based, must be within range)
	 */
	if ((NULL == payload) ||
	    ((uint8_t)3 > length) ||
	    (0U == payload[0]) ||
	    ((uint8_t)MAX_SPI_INTERFACES < payload[0]))
	{
		out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
		result = OUTPUT_ERR_INVALID_PARAM;
	}
	else
	{
		physical_cs = payload[0] - (uint8_t)1;

		/**
		 * @par Device type validation
		 * Checks if the device type is supported for LED output.
		 */
		if (((uint8_t)DEVICE_GENERIC_LED != device_config_map[physical_cs]) &&
		    ((uint8_t)DEVICE_TM1639_LED != device_config_map[physical_cs]))
		{
			out_statistics_counters.counters[OUT_CONTROLLER_ID_ERROR]++;
			result = OUTPUT_ERR_INVALID_PARAM;
		}
	}

	/**
	 * @par Mutex acquisition
	 * Tries to take the SPI mutex if parameters are valid.
	 */
	if (OUTPUT_OK == result)
	{
		if (pdTRUE == xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)))
		{
			/**
			 * @par LED processing and driver call
			 * Processes LED data and sends it to the driver if all checks passed.
			 */
			uint8_t index = payload[1];
			uint8_t ledstate = payload[2];

			output_driver_t *handle = output_drivers.driver_handles[physical_cs];
			if ((NULL != handle) && (NULL != handle->set_leds))
			{
				handle->set_leds(handle, index, ledstate);
			}
			else
			{
				(void)select_interface(physical_cs, false);
				out_statistics_counters.counters[OUT_DRIVER_INIT_ERROR]++;
				result = OUTPUT_ERR_DISPLAY_OUT;
			}
		}
		else
		{
			result = OUTPUT_ERR_SEMAPHORE;
		}
	}

	/**
	 * @par Mutex release
	 * Releases the SPI mutex (if it was taken).
	 */
	if (pdTRUE != xSemaphoreGive(spi_mutex))
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