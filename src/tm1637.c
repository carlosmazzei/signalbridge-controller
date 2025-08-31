/**
 * @file tm1637.c
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 * @brief Implementation of the TM1637 LED driver using GPIO bit-banging for Raspberry Pi Pico
 *
 * This driver utilizes GPIO bit-banging to implement the two-wire protocol (CLK and DIO)
 * required to control the TM1637 LED driver chip
 */

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include "outputs.h"
#include "tm1637.h"

/**
 * @brief Convert tm1637_result_t to output_result_t
 *
 * This function converts TM1637-specific error codes to generic output error codes
 * for consistent error handling throughout the system.
 *
 * @param[in] tm_result TM1637-specific result code
 * @return output_result_t Generic output result code
 */
static output_result_t tm1637_to_output_result(tm1637_result_t tm_result)
{
	output_result_t result;

	switch (tm_result)
	{
	case TM1637_OK:
		result = OUTPUT_OK;
		break;
	case TM1637_ERR_INVALID_PARAM:
		result = OUTPUT_ERR_INVALID_PARAM;
		break;
	default:
		result = OUTPUT_ERR_DISPLAY_OUT;
		break;
	}

	return result;
}


/**
 * @brief Start condition for TM1637 using SPI interface.
 *
 * This function selects the TM1637 chip via multiplexer using the existing SPI infrastructure.
 *
 * @param[in] config Pointer to the TM1637 output driver configuration structure.
 */
static inline void tm1637_start(const output_driver_t *config)
{
	config->select_interface(config->chip_id, true); // Select the chip via multiplexer
}

/**
 * @brief Stop condition for TM1637 using SPI interface.
 *
 * This function deselects the TM1637 chip via multiplexer.
 *
 * @param[in] config Pointer to the TM1637 output driver configuration structure.
 */
static inline void tm1637_stop(const output_driver_t *config)
{
	config->select_interface(config->chip_id, false); // Deselect the chip via multiplexer
}

/**
 * @brief Write a byte to the TM1637 using SPI interface.
 *
 * This function sends a single byte to the TM1637 device using the SPI hardware,
 * reusing the same pins and infrastructure as other SPI devices.
 *
 * @param[in] config Pointer to the TM1637 output driver configuration structure.
 * @param[in] data   Byte to be written to the TM1637.
 *
 * @return int Number of bytes written (should be 1 if successful).
 */
static inline int tm1637_write_byte(const output_driver_t *config, uint8_t data)
{
	// Use SPI hardware to write the byte (TM1637 will interpret as I2C-like protocol)
	int bytes_written = spi_write_blocking(config->spi, &data, 1);
	return bytes_written;
}

/**
 * @brief Send a command to the TM1637
 */
static tm1637_result_t tm1637_send_command(const output_driver_t *config, uint8_t cmd)
{
	tm1637_result_t result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		tm1637_start(config);
		int write_result = tm1637_write_byte(config, cmd);
		tm1637_stop(config);

		if (1 != write_result)
		{
			result = TM1637_ERR_WRITE;
		}
	}

	return result;
}

/**
 * @brief Set the display brightness level (0-7).
 *
 * This function sets the brightness level of the TM1637 display. The brightness value
 * is clamped to the range 0 to 7. The function updates the driver configuration and
 * sends the appropriate command to the TM1637 device.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 * @param[in]     level  Desired brightness level (0 = minimum, 7 = maximum).
 *
 * @return TM1637_OK on success,
 *         TM1637_ERR_INVALID_PARAM if config is NULL,
 *         or error code from tm1637_send_command.
 */
static tm1637_result_t tm1637_set_brightness(output_driver_t *config, uint8_t level)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		uint8_t set_level = (level > (uint8_t)7) ? (uint8_t)7 : level;
		config->brightness = set_level;

		// Display control command with brightness level
		uint8_t cmd = (uint8_t)TM1637_CMD_DISPLAY_ON | level;
		result = tm1637_send_command(config, cmd);
	}

	return result;
}

tm1637_result_t tm1637_display_on(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = true;
		result = tm1637_send_command(config, (uint8_t)TM1637_CMD_DISPLAY_ON | config->brightness);
	}

	return result;
}

output_driver_t* tm1637_init(uint8_t chip_id,
                             output_result_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin)
{
	output_driver_t *config = NULL;
	uint8_t valid = 1U;

	config = pvPortMalloc(sizeof(output_driver_t));
	if (NULL == config)
	{
		valid = 0U;
	}

	// Parameter validation
	if ((1U == valid) &&
	    ((chip_id >= (uint8_t)MAX_SPI_INTERFACES) ||
	     (NULL == select_interface) ||
	     (NULL == spi) ||
	     (dio_pin >= (uint8_t)NUM_GPIO) ||
	     (clk_pin >= (uint8_t)NUM_GPIO)))
	{
		valid = 0U;
	}

	// Only initialize if all parameters are valid
	if (1U == valid)
	{
		config->chip_id = chip_id;
		config->select_interface = select_interface;
		config->spi = spi;
		config->dio_pin = dio_pin;
		config->clk_pin = clk_pin;

		// Initialize buffer and state
		(void)memset(config->active_buffer, 0, sizeof(config->active_buffer));
		(void)memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
		config->buffer_modified = false;
		config->brightness = 7U;
		config->display_on = false;
		config->set_digits = &tm1637_set_digits;
		config->set_leds = &tm1637_set_leds;

		// Clear display on startup
		if (TM1637_OK != tm1637_clear(config))
		{
			valid = 0U;
		}

		// Set maximum brightness
		if (TM1637_OK != tm1637_set_brightness(config, 7U))
		{
			valid = 0U;
		}

		// Turn on display
		if (TM1637_OK != tm1637_display_on(config))
		{
			valid = 0U;
		}
	}

	// Free resources and set pointer to NULL on error
	if ((1U != valid) && (NULL != config))
	{
		vPortFree(config);
		config = NULL;
	}

	return config;
}

/**
 * @brief Update specific bytes in the preparation buffer
 */
static tm1637_result_t tm1637_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data)
{
	tm1637_result_t result = TM1637_OK;

	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else if (addr >= TM1637_DISPLAY_BUFFER_SIZE)
	{
		result = TM1637_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Update prep buffer and mark as modified
		config->prep_buffer[addr] = data;
		config->buffer_modified = true;
	}

	return result;
}

/**
 * @brief Flush the prepared buffer to the TM1637 display.
 *
 * This function copies the preparation buffer to the active buffer, resets the modified flag,
 * and writes all buffer data to the TM1637 device.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 *
 * @return TM1637_OK on success, error code otherwise
 */
static tm1637_result_t tm1637_flush(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;

	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Copy prep buffer to active buffer
		(void)memcpy(config->active_buffer, config->prep_buffer, TM1637_DISPLAY_BUFFER_SIZE);

		// Reset the modified flag
		config->buffer_modified = false;

		// TM1637 Protocol: Send data command first
		tm1637_start(config);
		if (tm1637_write_byte(config, TM1637_CMD_DATA_WRITE) != 1)
		{
			result = TM1637_ERR_WRITE;
		}
		tm1637_stop(config);

		// TM1637 Protocol: Send address and data
		if (TM1637_OK == result)
		{
			tm1637_start(config);
			
			// Send starting address (0xC0 for first digit)
			if (tm1637_write_byte(config, TM1637_CMD_ADDR_BASE) != 1)
			{
				result = TM1637_ERR_WRITE;
			}
			else
			{
				// Write all 4 digits data to TM1637
				for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
				{
					if (tm1637_write_byte(config, config->active_buffer[i]) != 1)
					{
						result = TM1637_ERR_WRITE;
					}
				}
			}
			tm1637_stop(config);
		}
	}

	return result;
}

/**
 * @brief Update the display with the current preparation buffer contents
 *
 * This function updates the display with the contents of the prep buffer.
 * Only updates if buffer_modified is true.
 *
 * @param config Pointer to TM1637 configuration structure
 * @return tm1637_result_t Error code, TM1637_OK if successful
 */
static tm1637_result_t tm1637_update(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Only update if buffer has been modified
		if (config->buffer_modified)
		{
			result = tm1637_flush(config);
		}
	}

	return result;
}

/**
 * @brief Validate custom character digit array
 * @param digits Pointer to custom digit array
 * @param length Number of digits to validate
 * @return TM1637_OK if all digits are valid, error code otherwise
 */
static tm1637_result_t tm1637_validate_custom_array(const uint8_t* digits, const size_t length)
{
	tm1637_result_t result = TM1637_OK;

	for (size_t i = 0U; (i < length) && (TM1637_OK == result); i++)
	{
		// Check if high nibble is zero and low nibble is valid (0-F)
		if ((digits[i] & 0xF0U) != 0x00U)
		{
			result = TM1637_ERR_INVALID_CHAR;
		}
	}

	return result;
}

/**
 * @brief Validate function parameters
 * @param[in] config Pointer to configuration
 * @param[in] digits Pointer to digit array
 * @param[in] length Array length
 * @param[in] dot_position Decimal point position
 * @return TM1637_OK if all parameters are valid, error code otherwise
 */
static tm1637_result_t tm1637_validate_parameters(const output_driver_t *config,
                                                  const uint8_t* digits,
                                                  const size_t length,
                                                  const uint8_t dot_position)
{
	tm1637_result_t result = TM1637_OK;

	// Single compound condition reduces cyclomatic complexity
	if ((NULL == config) ||
	    (NULL == digits) ||
	    (length != TM1637_DIGIT_COUNT) ||
	    ((dot_position > 3U) && (dot_position != TM1637_NO_DECIMAL_POINT)))
	{
		result = TM1637_ERR_INVALID_PARAM;
	}

	return result;
}

/**
 * @brief Process digits and update preparation buffer
 * @param[in,out] config Pointer to configuration
 * @param[in] digits Pointer to BCD digit array
 * @param[in] dot_position Decimal point position
 * @return TM1637_OK on success, error code otherwise
 */
static tm1637_result_t tm1637_process_digits(output_driver_t *config, const uint8_t* digits, const uint8_t dot_position)
{
	// Segment patterns for custom character set
	// Bits: dp-g-f-e-d-c-b-a (MSB to LSB)
	static const uint8_t tm1637_custom_patterns[16] = {
		0x3FU, 0x06U, 0x5BU, 0x4FU, // 0x00-0x03: 0, 1, 2, 3
		0x66U, 0x6DU, 0x7DU, 0x07U, // 0x04-0x07: 4, 5, 6, 7
		0x7FU, 0x6FU, 0x6DU, 0x1CU, // 0x08-0x0B: 8, 9, S, t
		0x5EU, 0x40U, 0x08U, 0x00U // 0x0C-0x0F: d, -, _, (blank)
	};

	tm1637_result_t result = TM1637_OK;

	for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
	{
		// Extract BCD digit and get pattern
		uint8_t bcd_digit = digits[i] & 0x0FU;
		uint8_t segment_data = tm1637_custom_patterns[bcd_digit];

		// Add decimal point using conditional expression (no if-statement)
		segment_data |= (i == dot_position) ? TM1637_DECIMAL_POINT_MASK : 0U;

		// Store in prep buffer
		config->prep_buffer[i] = segment_data;
	}

	if (TM1637_OK == result)
	{
		config->buffer_modified = true;
	}

	return result;
}

output_result_t tm1637_set_digits(output_driver_t *config,
                                  const uint8_t *digits,
                                  const size_t length,
                                  const uint8_t dot_position)
{
	tm1637_result_t tm_result;

	// Step 1: Validate parameters
	tm_result = tm1637_validate_parameters(config, digits, length, dot_position);

	// Step 2: Validate BCD encoding
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_validate_custom_array(digits, length);
	}

	// Step 3: Process digits
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_process_digits(config, digits, dot_position);
	}

	// Step 4: Update display
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_update(config);
	}

	return tm1637_to_output_result(tm_result);
}

output_result_t tm1637_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate)
{
	tm1637_result_t tm_result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		tm_result = TM1637_ERR_INVALID_PARAM;
	}
	else if (leds >= TM1637_DISPLAY_BUFFER_SIZE)
	{
		tm_result = TM1637_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Update preparation buffer with LED state
		tm_result = tm1637_update_buffer(config, leds, ledstate);

		// Update display if buffer update was successful
		if (TM1637_OK == tm_result)
		{
			tm_result = tm1637_update(config);
		}
	}

	return tm1637_to_output_result(tm_result);
}

tm1637_result_t tm1637_display_off(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = false;
		result = tm1637_send_command(config, TM1637_CMD_DISPLAY_OFF);
	}
	return result;
}

tm1637_result_t tm1637_clear(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Clear internal buffers first
		(void)memset(config->active_buffer, 0, TM1637_DISPLAY_BUFFER_SIZE);
		(void)memset(config->prep_buffer, 0, TM1637_DISPLAY_BUFFER_SIZE);
		config->buffer_modified = false;

		// Send clear data to display
		tm1637_start(config);
		if (TM1637_OK == tm1637_write_byte(config, TM1637_CMD_DATA_WRITE))
		{
			tm1637_stop(config);

			tm1637_start(config);
			if (TM1637_OK == tm1637_write_byte(config, TM1637_CMD_ADDR_BASE))
			{
				// Write zeros to clear all 4 digits
				for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
				{
					result = tm1637_write_byte(config, 0U);
				}
			}
			else
			{
				result = TM1637_ERR_WRITE;
			}
			tm1637_stop(config);
		}
		else
		{
			tm1637_stop(config);
			result = TM1637_ERR_WRITE;
		}
	}

	return result;
}

tm1637_result_t tm1637_deinit(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (!config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Turn off display
		result = tm1637_display_off(config);
		
		// Reset GPIO pins to safe state
		gpio_put(config->dio_pin, 0);
		gpio_put(config->clk_pin, 0);
		gpio_deinit(config->dio_pin);
		gpio_deinit(config->clk_pin);
	}
	return result;
}