/**
 * @file app_outputs.c
 * @brief Implementation of output control functions: LEDs, PWM, and 7-segment displays.
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 * @date 2020-2025
 *
 * This file contains functions responsible for initializing and controlling output devices,
 * including LEDs, PWM, and 7-segment displays, using specific drivers (e.g., TM1639) and
 * SPI multiplexing. It also manages error statistics and synchronization for SPI bus access.
 *
 * @copyright
 * (c) 2020-2025 Carlos Mazzei. All rights reserved.
 */

#include <string.h>

#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <pico/binary_info.h>
#include <pico/stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "tm1639.h"
#include "tm1637.h"
#include "app_outputs.h"
#include "error_management.h"

/**
 * @brief Device configuration map for all SPI interfaces.
 *
 * Mirrors @ref DEVICE_CONFIG so the implementation can reason about the
 * concrete driver assigned to each logical slot.
 */
static const uint8_t device_config_map[MAX_SPI_INTERFACES] = DEVICE_CONFIG;

/**
 * @brief Mutex for synchronizing access to SPI operations.
 *
 * This semaphore ensures that only one task accesses the SPI bus at a time.
 */
static SemaphoreHandle_t spi_mutex = NULL;

/**
 * @brief Structure holding all output driver handles (module scope).
 */
static output_drivers_t output_drivers;

/**
 * @brief Initialise GPIO used by the SPI multiplexer.
 *
 * @retval OUTPUT_OK        GPIOs configured successfully.
 * @retval OUTPUT_ERR_INIT  One or more GPIOs failed the post-configuration check.
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
		statistics_increment_counter(OUTPUT_INIT_ERROR);
		output_result = OUTPUT_ERR_INIT;
	}
	else
	{
		output_result = OUTPUT_OK;
	}

	return output_result;
}

/**
 * @brief Toggle the multiplexer lines for the requested device.
 *
 * @param[in] chip_select Chip select number (0-7).
 * @param[in] select      `true` to assert the strobe, `false` to release it.
 *
 * @retval OUTPUT_OK            Multiplexer state updated.
 * @retval OUTPUT_ERR_INVALID_PARAM Chip identifier outside the supported range.
 */
static output_result_t select_interface(uint8_t chip_select, bool select)
{
	output_result_t result = OUTPUT_OK;

	// Check if chip_select is within valid range
	if (chip_select >= (uint8_t)MAX_SPI_INTERFACES)
	{
		result = OUTPUT_ERR_INVALID_PARAM;
		statistics_increment_counter(OUTPUT_INVALID_PARAM_ERROR);
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
 * @brief Instantiate driver back-ends based on @ref device_config_map.
 *
 * @retval OUTPUT_OK        All configured drivers initialised successfully.
 * @retval OUTPUT_ERR_INIT  At least one driver failed to allocate resources.
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
				statistics_increment_counter(OUTPUT_DRIVER_INIT_ERROR);
				continue;
			}
		}
		else if (((uint8_t)DEVICE_TM1637_DIGIT == device_config_map[i]) ||
		         ((uint8_t)DEVICE_TM1637_LED == device_config_map[i]))
		{
			// Initialize TM1637 driver using same SPI infrastructure as TM1639
			output_drivers.driver_handles[i] = tm1637_init(i,
			                                               &select_interface,
			                                               spi0,
			                                               PICO_DEFAULT_SPI_TX_PIN,
			                                               PICO_DEFAULT_SPI_SCK_PIN);
			if (!output_drivers.driver_handles[i])
			{
				result = OUTPUT_ERR_INIT;
				statistics_increment_counter(OUTPUT_DRIVER_INIT_ERROR);
				continue;
			}
		}
		else if (((uint8_t)DEVICE_GENERIC_DIGIT == device_config_map[i]) ||
		         ((uint8_t)DEVICE_GENERIC_LED == device_config_map[i]))
		{
			// Initialize generic driver (if needed)
		}
	}

	return result;
}

/**
 * @brief Initialise UART0 on GPIO 12 (TX) and GPIO 13 (RX).
 *
 * @param[in] baudrate UART baud rate (e.g. 115200).
 */
static void uart0_init(uint32_t baudrate)
{
	// Initialize UART0 hardware
	uart_init(uart0, baudrate);

	// Set GPIO 12 as UART0 TX
	gpio_set_function(12, GPIO_FUNC_UART);

	// Set GPIO 13 as UART0 RX
	gpio_set_function(13, GPIO_FUNC_UART);

	// Optional: Enable FIFO
	uart_set_fifo_enabled(uart0, true);
}

output_result_t output_init(void)
{
	output_result_t result = OUTPUT_OK;

	// Create mutex
	if (!spi_mutex)
	{
		spi_mutex = xSemaphoreCreateMutex();
		// Error returning mutex
		if (!spi_mutex)
		{
			result = OUTPUT_ERR_INIT;
			statistics_increment_counter(OUTPUT_INIT_ERROR);
		}
	}

	// Initialize multiplexer
	output_result_t result_mux = init_mux();
	if (result_mux != OUTPUT_OK)
	{
		statistics_increment_counter(OUTPUT_INIT_ERROR);
	}

	// Initialize SPI
	spi_init(spi0, SPI_FREQUENCY);
	spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);

	gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

	// Verify SPI pin configuration
	if ((gpio_get_function(PICO_DEFAULT_SPI_SCK_PIN) != GPIO_FUNC_SPI) ||
	    (gpio_get_function(PICO_DEFAULT_SPI_TX_PIN) != GPIO_FUNC_SPI))
	{
		statistics_increment_counter(OUTPUT_INIT_ERROR);
		result =  OUTPUT_ERR_INIT;
	}

	// Make the SPI pins available to picotool
	bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI))

	// Initialize UART0
	uart0_init(115200);

	// Configure PWM pin
	gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
	pwm_config config = pwm_get_default_config();
	pwm_config_set_clkdiv(&config, 10.f);
	pwm_init(slice_num, &config, true);

	output_result_t result_init = init_driver();
	if (result_init != OUTPUT_OK)
	{
		statistics_increment_counter(OUTPUT_DRIVER_INIT_ERROR);
	}

	return result;
}

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
		statistics_increment_counter(OUTPUT_CONTROLLER_ID_ERROR);
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
		    ((uint8_t)DEVICE_TM1639_DIGIT != device_config_map[physical_cs]) &&
		    ((uint8_t)DEVICE_TM1637_DIGIT != device_config_map[physical_cs]))
		{
			statistics_increment_counter(OUTPUT_CONTROLLER_ID_ERROR);
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
			result = handle->set_digits(handle, digits, sizeof(digits), payload[5]);
		}
		else
		{
			// If the driver handle is invalid, try to deselect the chip and set error
			(void)select_interface(physical_cs, false);
			statistics_increment_counter(OUTPUT_DRIVER_INIT_ERROR);
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
		statistics_increment_counter(OUTPUT_CONTROLLER_ID_ERROR);
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
		    ((uint8_t)DEVICE_TM1639_LED != device_config_map[physical_cs]) &&
		    ((uint8_t)DEVICE_TM1637_LED != device_config_map[physical_cs]))
		{
			statistics_increment_counter(OUTPUT_CONTROLLER_ID_ERROR);
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
				result = handle->set_leds(handle, index, ledstate);
			}
			else
			{
				(void)select_interface(physical_cs, false);
				statistics_increment_counter(OUTPUT_DRIVER_INIT_ERROR);
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
