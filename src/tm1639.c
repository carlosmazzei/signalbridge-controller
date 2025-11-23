/**
 * @file tm1639.c
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 * @brief Implementation of the TM1639 LED driver using SPI hardware for Raspberry Pi Pico
 *
 * This driver utilizes the Raspberry Pi Pico's SPI0 hardware interface to control
 * the TM1639 LED driver chip
 */

 #include <string.h>

 #include <pico/stdlib.h>
 #include <hardware/spi.h>
 #include <hardware/gpio.h>
 #include <hardware/clocks.h>
 #include <hardware/pio.h>

 #include "FreeRTOS.h"
 #include "semphr.h"

 #include "tm1639.h"

/*
 * Forward declarations
 */
static output_result_t tm1639_to_output_result(tm1639_result_t tm_result);
static tm1639_result_t tm1639_send_command(const output_driver_t *config, uint8_t cmd);
static tm1639_result_t tm1639_set_brightness(output_driver_t *config, uint8_t level);
static inline void tm1639_start(const output_driver_t *config);
static inline void tm1639_stop(const output_driver_t *config);
static inline int tm1639_write_byte(const output_driver_t *config, uint8_t data);
static void tm1639_set_read_mode(const output_driver_t *config);
static inline void tm1639_set_write_mode(const output_driver_t *config);
static tm1639_result_t tm1639_read_bytes(const output_driver_t *config, uint8_t *data, uint8_t count);
static tm1639_result_t tm1639_clear_and_off(output_driver_t *config);
static tm1639_result_t tm1639_flush(output_driver_t *config);
static tm1639_result_t tm1639_set_address(const output_driver_t *config, uint8_t addr);
static tm1639_result_t tm1639_write_data_at(output_driver_t *config, uint8_t addr, uint8_t data);
static tm1639_result_t tm1639_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data);
static tm1639_result_t tm1639_write_data(output_driver_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length);
static tm1639_result_t tm1639_read_keys(const output_driver_t *config, uint8_t *key_data);
static tm1639_result_t tm1639_get_key_states(const output_driver_t *config, tm1639_key_t *keys);
static tm1639_result_t tm1639_update(output_driver_t *config);
static tm1639_result_t tm1639_validate_custom_array(const uint8_t *digits, const size_t length);
static tm1639_result_t tm1639_validate_parameters(const output_driver_t *config,
                                                  const uint8_t *digits,
                                                  const size_t length,
                                                  const uint8_t dot_position);
static tm1639_result_t tm1639_process_digits(output_driver_t *config, const uint8_t *digits, const uint8_t dot_position);

/**
 * @brief Convert tm1639_result_t to output_result_t
 *
 * This function converts TM1639-specific error codes to generic output error codes
 * for consistent error handling throughout the system.
 *
 * @param[in] tm_result TM1639-specific result code
 * @return output_result_t Generic output result code
 */
static output_result_t tm1639_to_output_result(tm1639_result_t tm_result)
{
	output_result_t result;

	switch (tm_result)
	{
	case TM1639_OK:
		result = OUTPUT_OK;
		break;
	case TM1639_ERR_INVALID_PARAM:
		result = OUTPUT_ERR_INVALID_PARAM;
		break;
	default:
		result = OUTPUT_ERR_DISPLAY_OUT;
		break;
	}

	return result;
}

/**
 * @brief Set the display brightness level (0-7).
 *
 * This function sets the brightness level of the TM1639 display. The brightness value
 * is clamped to the range 0 to 7. The function updates the driver configuration and
 * sends the appropriate command to the TM1639 device.
 *
 * @param[in,out] config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[in]     level  Desired brightness level (0 = minimum, 7 = maximum).
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         or error code from tm1639_send_command.
 */
static tm1639_result_t tm1639_set_brightness(output_driver_t *config, uint8_t level)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		uint8_t set_level = (level > (uint8_t)7) ? (uint8_t)7 : level;
		config->brightness = set_level;

		// Display control command with brightness level
		uint8_t cmd = (uint8_t)TM1639_CMD_DISPLAY_ON | level;
		result = tm1639_send_command(config, cmd);
	}

	return result;
}

/**
 * @brief Start a transmission by setting STB low via multiplexer.
 *
 * This function selects the TM1639 chip by setting the strobe (STB) line low
 * using the provided select_interface function pointer.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 */
static inline void tm1639_start(const output_driver_t *config)
{
	config->select_interface(config->chip_id, true); // Select the chip (STB low)
}

/**
 * @brief End a transmission by setting STB high via multiplexer.
 *
 * This function deselects the TM1639 chip by setting the strobe (STB) line high
 * using the provided select_interface function pointer.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 */
static inline void tm1639_stop(const output_driver_t *config)
{
	config->select_interface(config->chip_id, false); // Deselect the chip (STB high)
}

/**
 * @brief Write a byte to the TM1639 using SPI.
 *
 * This function sends a single byte to the TM1639 device using the configured SPI hardware.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 * @param[in] data   Byte to be written to the TM1639.
 *
 * @return Number of bytes written (1 on success).
 */
static inline int tm1639_write_byte(const output_driver_t *config, uint8_t data)
{
	// Use SPI hardware to write the byte
	int bytes_written = spi_write_blocking(config->spi, &data, 1);
	return bytes_written;
}

/**
 * @brief Configure GPIO for reading from the DIO pin.
 *
 * This function sets the DIO pin to input mode and disables its SPI function,
 * allowing bit-banged read operations from the TM1639. It also sets the clock pin
 * to output mode and enables pull-up resistors on both pins, as required by the TM1639 protocol.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 */
static void tm1639_set_read_mode(const output_driver_t *config)
{
	// Disable SPI function for DIO pin
	gpio_set_function(config->dio_pin, GPIO_FUNC_NULL);
	gpio_set_dir(config->dio_pin, GPIO_IN);

	// Toggle clock pin directly, as we're not in SPI mode now
	gpio_set_function(config->clk_pin, GPIO_FUNC_NULL);
	gpio_set_dir(config->clk_pin, GPIO_OUT);

	// Enable pull-up on DIO pin (TM1639 uses open drain outputs)
	gpio_pull_up(config->dio_pin);
	gpio_pull_up(config->clk_pin);
}

/**
 * @brief Configure GPIO back to SPI mode for writing.
 *
 * This function restores the DIO and CLK pins to SPI mode,
 * allowing hardware SPI operations to the TM1639 device.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 */
static inline void tm1639_set_write_mode(const output_driver_t *config)
{
	// Restore SPI function for DIO pin
	gpio_set_function(config->dio_pin, GPIO_FUNC_SPI);
	gpio_set_function(config->clk_pin, GPIO_FUNC_SPI);
}

/**
 * @brief Bit-bang read operation for TM1639.
 *
 * This function performs a bit-banged read operation to retrieve bytes from the TM1639 device.
 * It switches the DIO pin to input mode, reads the specified number of bytes, and then restores
 * the pins to SPI mode.
 *
 * @param[in]  config Pointer to the TM1639 output driver configuration structure.
 * @param[out] data   Pointer to the buffer where the read bytes will be stored.
 * @param[in]  count  Number of bytes to read from the TM1639.
 *
 * @return TM1639_OK on success, or TM1639_ERR_INVALID_PARAM if parameters are invalid.
 */
static tm1639_result_t tm1639_read_bytes(const output_driver_t *config, uint8_t *data, uint8_t count)
{
	tm1639_result_t result = TM1639_OK;

	// Parameter validation
	if ((NULL == config) || (NULL == data))
	{
		result = TM1639_ERR_INVALID_PARAM;
	}

	if (TM1639_OK == result)
	{
		// Switch DIO to input mode
		tm1639_set_read_mode(config);

		// Waiting time (Twait) required by TM1639 protocol - at least 2µs
		sleep_us(2);

		// Manual bit-banging to read data
		for (uint8_t i = 0U; i < count; i++)
		{
			data[i] = 0U;

			// Manual bit-banging of clock to read 8 bits
			for (uint8_t bit = 0U; bit < (uint8_t)8; bit++)
			{
				// Set clock high
				gpio_put(config->clk_pin, 1);
				sleep_us(1); // Brief delay

				// Read data bit (LSB first)
				if (0U != gpio_get(config->dio_pin))
				{
					data[i] |= ((uint8_t)1U << bit);
				}

				// Set clock low
				gpio_put(config->clk_pin, 0);
				sleep_us(1); // Brief delay
			}
		}

		// Restore pins to SPI mode
		tm1639_set_write_mode(config);
	}

	return result;
}

/** 
 * @brief Clear the TM1639 display and turn it off.
 *
 * @param[in,out] config Pointer to the TM1639 driver configuration.
 *
 * @return TM1639_OK on success or an error code describing the failure.
 */
static tm1639_result_t tm1639_clear_and_off(output_driver_t *config)
{
	tm1639_result_t result;

	// Clear the display first
	result = tm1639_clear(config);

	// Turn off display if clear was successful
	if (TM1639_OK == result)
	{
		result = tm1639_display_off(config);
	}

	return result;
}

/**
 * @brief Flush the prepared buffer to the TM1639 display.
 *
 * This function copies the preparation buffer to the active buffer, resets the modified flag,
 * and writes all buffer data to the TM1639 device using auto-increment mode.
 *
 * @param[in,out] config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         TM1639_ERR_SPI_WRITE if a SPI write operation fails.
 */
static tm1639_result_t tm1639_flush(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;

	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		// Copy prep buffer to active buffer
		(void)memcpy(config->active_buffer, config->prep_buffer, sizeof(config->active_buffer));

		// Reset the modified flag
		config->buffer_modified = false;

		// Step 1: Send data command for auto-increment mode
		tm1639_start(config); // STB goes low

		// Auto-increment mode, write data: 01000000 (0x40)
		if (tm1639_write_byte(config, TM1639_CMD_DATA_WRITE) != 1)
		{
			result = TM1639_ERR_SPI_WRITE;
		}

		tm1639_stop(config); // STB goes high - Required between commands

		// Step 2: Send address command and write all buffer data
		if (TM1639_OK == result)
		{
			tm1639_start(config); // STB goes low again

			// Set starting address to 0x00: 11000000 (0xC0)
			if (tm1639_write_byte(config, TM1639_CMD_ADDR_BASE) != 1)
			{
				result = TM1639_ERR_SPI_WRITE;
			}
			else
			{
				// Write all 16 bytes from active buffer
				// MISRA-C: Declare loop variable with reduced scope
				for (uint8_t i = 0U; (i < 16U) && (TM1639_OK == result); i++)
				{
					if (tm1639_write_byte(config, config->active_buffer[i]) != 1)
					{
						result = TM1639_ERR_SPI_WRITE;
						// Break handled by loop condition
					}
				}
			}
			tm1639_stop(config); // STB goes high to end transaction
		}
		else
		{
			// Ensure STB is properly handled on error path
			tm1639_stop(config);
		}
	}

	return result;
}

/**
 * @brief Transmit a raw command to the TM1639 controller.
 *
 * @param[in] config Driver handle obtained from @ref tm1639_init().
 * @param[in] cmd    Encoded command byte to be written on the bus.
 *
 * @retval TM1639_OK              The command was accepted by the controller.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_SPI_WRITE   The byte transfer failed.
 */
static tm1639_result_t tm1639_send_command(const output_driver_t *config, uint8_t cmd)
{
	tm1639_result_t result = TM1639_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		tm1639_start(config);
		int write_result = tm1639_write_byte(config, cmd);
		tm1639_stop(config);

		if (1 != write_result)
		{
			result = TM1639_ERR_SPI_WRITE;
		}
	}

	return result;
}

/**
 * @brief Select the TM1639 display memory address for subsequent writes.
 *
 * @param[in] config Driver handle obtained from @ref tm1639_init().
 * @param[in] addr   Address in the range 0-15 to program on the device.
 *
 * @retval TM1639_OK              The address command was accepted.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_ADDRESS_RANGE The address exceeds the valid range.
 * @retval TM1639_ERR_SPI_WRITE   The command write failed.
 */
static tm1639_result_t tm1639_set_address(const output_driver_t *config, uint8_t addr)
{
	tm1639_result_t result = TM1639_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else if ((uint8_t)0x0F < addr)
	{
		result = TM1639_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Address command: B7:B6 = 11, Address in B3:B0
		uint8_t cmd = (uint8_t)TM1639_CMD_ADDR_BASE | (uint8_t)(addr & (uint8_t)0x0F);
		result = tm1639_send_command(config, cmd);
	}

	return result;
}

/**
 * @brief Write a single byte to a fixed TM1639 display register.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 * @param[in]     addr   Display memory address to update (0-15).
 * @param[in]     data   Segment pattern to store at @p addr.
 *
 * @retval TM1639_OK              The byte was written successfully.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_ADDRESS_RANGE The address exceeds the valid range.
 * @retval TM1639_ERR_SPI_WRITE   The underlying SPI transaction failed.
 */
static tm1639_result_t tm1639_write_data_at(output_driver_t *config, uint8_t addr, uint8_t data)
{
	uint8_t result = TM1639_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else if (addr > 0x0FU)
	{
		result = TM1639_ERR_ADDRESS_RANGE;
	}
	else
	{
		tm1639_start(config);
		// Set fixed address mode
		int write_result = tm1639_write_byte(config, TM1639_CMD_FIXED_ADDR); // DATA_CMD_FIXED_ADDR
		tm1639_stop(config);

		if (1 == write_result)
		{
			tm1639_start(config);
			// Set the address
			write_result = tm1639_write_byte(config, (uint8_t)((uint8_t)TM1639_CMD_ADDR_BASE | (addr & 0x0FU)));
			if (1 == write_result)
			{
				// Write the data
				write_result = tm1639_write_byte(config, data);
				if (1 == write_result)
				{
					// Update active buffer
					config->active_buffer[addr] = data;
				}
				else
				{
					result = TM1639_ERR_SPI_WRITE; // Error writing data
				}
			}
		}
		tm1639_stop(config);
	}

	return result;
}

/**
 * @brief Update a single byte in the TM1639 preparation buffer.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 * @param[in]     addr   Display memory address to update (0-15).
 * @param[in]     data   Segment pattern to cache at @p addr.
 *
 * @retval TM1639_OK              The cache entry was updated successfully.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_ADDRESS_RANGE The address exceeds the valid range.
 */
static tm1639_result_t tm1639_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data)
{
	tm1639_result_t result = TM1639_OK;

	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else if (addr > 0x0FU)
	{
		result = TM1639_ERR_ADDRESS_RANGE;
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
 * @brief Write multiple bytes to the TM1639 using auto-increment mode.
 *
 * @param[in,out] config     Driver handle obtained from @ref tm1639_init().
 * @param[in]     addr       Start address in display memory (0-15).
 * @param[in]     data_bytes Pointer to the data block to send.
 * @param[in]     length     Number of bytes contained in @p data_bytes.
 *
 * @retval TM1639_OK              The transfer completed successfully.
 * @retval TM1639_ERR_INVALID_PARAM One or more arguments are invalid.
 * @retval TM1639_ERR_ADDRESS_RANGE The address and length exceed the buffer size.
 * @retval TM1639_ERR_SPI_WRITE   The SPI transaction failed.
 */
static tm1639_result_t tm1639_write_data(output_driver_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length)
{
	tm1639_result_t result = TM1639_OK;

	// Parameter validation
	if ((NULL == config) || (NULL == data_bytes) || (0U == length))
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else if ((addr > 0x0FU) || ((uint16_t)addr + (uint16_t)length > 16U))
	{
		result = TM1639_ERR_ADDRESS_RANGE;
	}
	else
	{
		tm1639_start(config);

		// Set auto-increment mode
		int write_result = tm1639_write_byte(config, TM1639_CMD_DATA_WRITE); // DATA_CMD_AUTO_INC
		if (1 == write_result)
		{
			// Set the starting address
			write_result = tm1639_write_byte(config, (uint8_t)(0xC0U | (addr & 0x0FU)));
			if (1 == write_result)
			{
				// Write all data bytes
				for (uint8_t i = 0U; i < length; i++)
				{
					write_result = tm1639_write_byte(config, data_bytes[i]);
					if (write_result != 1)
					{
						result = TM1639_ERR_SPI_WRITE;
						break;
					}
					config->active_buffer[addr + i] = data_bytes[i];
				}
			}
		}
		tm1639_stop(config);
	}
	return result;
}

/**
 * @brief Read the key scan data from the TM1639 device.
 *
 * This function reads the key scan data (2 bytes) from the TM1639 device into the provided buffer.
 *
 * @param[in,out] config   Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[out]    key_data Pointer to the buffer where the 2 bytes of key scan data will be stored. Must not be NULL.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if any parameter is invalid,
 *         or error code from tm1639_write_byte or tm1639_read_bytes.
 */
static tm1639_result_t tm1639_read_keys(const output_driver_t *config, uint8_t *key_data)
{
	tm1639_result_t result = TM1639_OK;

	// Parameter validation
	if ((NULL == config) || (NULL == key_data))
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		tm1639_start(config);
		// Set key reading mode
		if (tm1639_write_byte(config, TM1639_CMD_DATA_READ_KEYS) != 1)
		{
			result = TM1639_ERR_SPI_WRITE;
		}
		else
		{
			// Read key scan data (2 bytes)
			result = tm1639_read_bytes(config, key_data, 2U);
		}
		tm1639_stop(config);
	}

	return result;
}

/**
 * @brief Decode the pressed key states reported by the TM1639.
 *
 * The caller must provide storage for four @ref tm1639_key_t entries. The
 * driver populates the array sequentially with any active key scans and leaves
 * unused entries unchanged.
 *
 * @param[in]  config Driver handle obtained from @ref tm1639_init().
 * @param[out] keys   Array that receives the decoded key descriptors.
 *
 * @retval TM1639_OK              Key data was retrieved successfully.
 * @retval TM1639_ERR_INVALID_PARAM @p config or @p keys is NULL.
 * @retval TM1639_ERR_SPI_WRITE   The read transaction failed.
 */
static tm1639_result_t tm1639_get_key_states(const output_driver_t *config, tm1639_key_t *keys)
{
	tm1639_result_t result = TM1639_OK;

	if ((NULL == config) || (NULL == keys))
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		uint8_t key_data[2] = {0U, 0U};
		result = tm1639_read_keys(config, key_data);

		if (TM1639_OK == result)
		{
			uint8_t count = 0U;
			for (uint8_t ks_idx = 0U; ks_idx < 2U; ks_idx++)
			{
				uint8_t byte = key_data[ks_idx];
				uint8_t ks_base = (uint8_t)((ks_idx * 2U) + 1U);

				if (0U != (byte & 0x04U))
				{
					keys[count].ks = ks_base;
					keys[count].k = 0U;
					count++;
				}
				if (0U != (byte & 0x08U))
				{
					keys[count].ks = ks_base;
					keys[count].k = 1U;
					count++;
				}
				if (0U != (byte & 0x40U))
				{
					keys[count].ks = (uint8_t)(ks_base + 1U);
					keys[count].k = 0U;
					count++;
				}
				if (0U != (byte & 0x80U))
				{
					keys[count].ks = (uint8_t)(ks_base + 1U);
					keys[count].k = 1U;
					count++;
				}
			}
		}
	}

	return result;
}

/**
 * @brief Update the display with the current preparation buffer contents.
 *
 * This function updates the display with the contents of the preparation buffer
 * when it has been marked as modified.
 *
 * @param[in,out] config TM1639 driver configuration.
 *
 * @return TM1639_OK if the display was updated successfully, error code otherwise.
 */
static tm1639_result_t tm1639_update(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		// Only update if buffer has been modified
		if (config->buffer_modified)
		{
			result = tm1639_flush(config);
		}
	}

	return result;
}

/**
 * @brief Validate a custom character digit array.
 *
 * @param[in] digits Pointer to the digit array to validate.
 * @param[in] length Number of digits provided in @p digits.
 *
 * @return TM1639_OK if every entry is valid, error code otherwise.
 */
static tm1639_result_t tm1639_validate_custom_array(const uint8_t* digits, const size_t length)
{
	tm1639_result_t result = TM1639_OK;

	for (size_t i = 0U; (i < length) && (TM1639_OK == result); i++)
	{
		// Check if high nibble is zero and low nibble is valid (0-F)
		if ((digits[i] & 0xF0U) != 0x00U)
		{
			result = TM1639_ERR_INVALID_CHAR;
		}
	}

	return result;
}

/**
 * @brief Validate TM1639 digit update parameters.
 *
 * @param[in] config        TM1639 driver configuration.
 * @param[in] digits        Pointer to the BCD digit array.
 * @param[in] length        Number of entries stored in @p digits.
 * @param[in] dot_position  Decimal point position or TM1639_NO_DECIMAL_POINT.
 *
 * @return TM1639_OK if all parameters are valid, error code otherwise.
 */
static tm1639_result_t tm1639_validate_parameters(const output_driver_t *config,
                                                  const uint8_t* digits,
                                                  const size_t length,
                                                  const uint8_t dot_position)
{
	tm1639_result_t result = TM1639_OK;

	// Single compound condition reduces cyclomatic complexity
	if ((NULL == config) ||
	    (NULL == digits) ||
	    (length != TM1639_DIGIT_COUNT) ||
	    ((dot_position > 7U) && (dot_position != TM1639_NO_DECIMAL_POINT)))
	{
		result = TM1639_ERR_INVALID_PARAM;
	}

	return result;
}

/**
 * @brief Convert BCD digits into TM1639 segment data.
 *
 * @param[in,out] config      TM1639 driver configuration.
 * @param[in]     digits      Pointer to the BCD digit array.
 * @param[in]     dot_position Decimal point position or TM1639_NO_DECIMAL_POINT.
 *
 * @return TM1639_OK when the preparation buffer was updated successfully.
 */
static tm1639_result_t tm1639_process_digits(output_driver_t *config, const uint8_t* digits, const uint8_t dot_position)
{
	// Segment patterns for custom character set
	// Bits: dp-g-f-e-d-c-b-a (MSB to LSB)
	static const uint8_t tm1639_custom_patterns[16] = {
		0x3FU, 0x06U, 0x5BU, 0x4FU, // 0x00-0x03: 0, 1, 2, 3
		0x66U, 0x6DU, 0x7DU, 0x07U, // 0x04-0x07: 4, 5, 6, 7
		0x7FU, 0x6FU, 0x6DU, 0x1CU, // 0x08-0x0B: 8, 9, S, t
		0x5EU, 0x40U, 0x08U, 0x00U // 0x0C-0x0F: d, -, _, (blank)
	};

	tm1639_result_t result = TM1639_OK;

	for (uint8_t i = 0U; (i < TM1639_DIGIT_COUNT) && (TM1639_OK == result); i++)
	{
		// Extract BCD digit and get pattern
		uint8_t bcd_digit = digits[i] & 0x0FU;
		uint8_t segment_data = tm1639_custom_patterns[bcd_digit];

		// Add decimal point using conditional expression (no if-statement)
		segment_data |= (i == dot_position) ? TM1639_DECIMAL_POINT_MASK : 0U;

		// Calculate address and validate bounds
		uint8_t addr = i * 2U;
		if (addr < TM1639_DISPLAY_BUFFER_SIZE)
		{
			config->prep_buffer[addr] = segment_data;
		}
		else
		{
			result = TM1639_ERR_ADDRESS_RANGE;
		}
	}

	if (TM1639_OK == result)
	{
		config->buffer_modified = true;
	}

	return result;
}

output_driver_t* tm1639_init(uint8_t chip_id,
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
		config->set_digits = &tm1639_set_digits;
		config->set_leds = &tm1639_set_leds;

		// Clear display on startup
		if (TM1639_OK != tm1639_clear(config))
		{
			valid = 0U;
		}

		// Set maximum brightness
		if (TM1639_OK != tm1639_set_brightness(config, 7U))
		{
			valid = 0U;
		}

		// Turn on display
		if (TM1639_OK != tm1639_display_on(config))
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

tm1639_result_t tm1639_display_on(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = true;
		result = tm1639_send_command(config, (uint8_t)TM1639_CMD_DISPLAY_ON | config->brightness); // DISPLAY_ON | brightness
	}

	return result;
}

tm1639_result_t tm1639_display_off(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = false;
		result = tm1639_send_command(config, TM1639_CMD_DISPLAY_OFF); // DISPLAY_OFF
	}
	return result;
}

output_result_t tm1639_set_digits(output_driver_t *config,
                                  const uint8_t *digits,
                                  const size_t length,
                                  const uint8_t dot_position)
{
	tm1639_result_t tm_result;

	// Step 1: Validate parameters
	tm_result = tm1639_validate_parameters(config, digits, length, dot_position);

	// Step 2: Validate BCD encoding
	if (TM1639_OK == tm_result)
	{
		tm_result = tm1639_validate_custom_array(digits, length);
	}

	// Step 3: Process digits
	if (TM1639_OK == tm_result)
	{
		tm_result = tm1639_process_digits(config, digits, dot_position);
	}

	// Step 4: Update display
	if (TM1639_OK == tm_result)
	{
		tm_result = tm1639_update(config);
	}

	return tm1639_to_output_result(tm_result);
}

tm1639_result_t tm1639_clear(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;

	// MISRA-C: Parameter validation
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		// Clear internal buffers first
		(void)memset(config->active_buffer, 0, sizeof(config->active_buffer));
		(void)memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
		config->buffer_modified = false;

		// Step 1: Send data command for auto-increment mode
		tm1639_start(config); // STB goes low

		// Auto-increment mode, write data: 01000000 (0x40)
		if (tm1639_write_byte(config, TM1639_CMD_DATA_WRITE) != 1)
		{
			result = TM1639_ERR_SPI_WRITE;
		}

		tm1639_stop(config); // STB goes high - Required between commands

		// Step 2: Send address command and clear all 16 registers
		if (TM1639_OK == result)
		{
			tm1639_start(config); // STB goes low again

			// Set starting address to 0x00: 11000000 (0xC0)
			if (tm1639_write_byte(config, TM1639_CMD_ADDR_BASE) != 1)
			{
				result = TM1639_ERR_SPI_WRITE;
			}
			else
			{
				// Write 16 zero bytes to clear all display registers
				// MISRA-C: Declare loop variable with reduced scope
				for (uint8_t i = 0U; (i < 16U) && (TM1639_OK == result); i++)
				{
					if (tm1639_write_byte(config, 0U) != 1)
					{
						result = TM1639_ERR_SPI_WRITE;
						// Break handled by loop condition
					}
				}
			}

			tm1639_stop(config); // STB goes high to end transaction
		}
	}

	return result;
}

output_result_t tm1639_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate)
{
	tm1639_result_t tm_result = TM1639_OK;

	// Parameter validation
	if (NULL == config)
	{
		tm_result = TM1639_ERR_INVALID_PARAM;
	}
	else if (leds > 0x0FU)
	{
		tm_result = TM1639_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Update preparation buffer with LED state
		tm_result = tm1639_update_buffer(config, leds, ledstate);

		// Update display if buffer update was successful
		if (TM1639_OK == tm_result)
		{
			tm_result = tm1639_update(config);
		}
	}

	return tm1639_to_output_result(tm_result);
}

tm1639_result_t tm1639_deinit(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;
	if (!config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		// Turn off display
		result = tm1639_display_off(config);
	}
	return result;
}
