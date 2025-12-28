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
static inline void tm1639_start(const output_driver_t *config);
static inline void tm1639_stop(const output_driver_t *config);
static inline int tm1639_write_byte(const output_driver_t *config, uint8_t data);
static void tm1639_set_read_mode(const output_driver_t *config);
static inline void tm1639_set_write_mode(const output_driver_t *config);
static tm1639_result_t tm1639_read_bytes(const output_driver_t *config, uint8_t *data, uint8_t count);
static tm1639_result_t tm1639_flush(output_driver_t *config);
static tm1639_result_t tm1639_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data);
static tm1639_result_t tm1639_get_key_states(const output_driver_t *config, tm1639_key_t *keys);
static tm1639_result_t tm1639_update(output_driver_t *config);
static tm1639_result_t tm1639_validate_custom_array(const uint8_t *digits, const size_t length);
static tm1639_result_t tm1639_validate_parameters(const output_driver_t *config,
                                                  const uint8_t *digits,
                                                  const size_t length,
                                                  const uint8_t dot_position);
static tm1639_result_t tm1639_process_digits(output_driver_t *config, const uint8_t *digits, const uint8_t dot_position);
static tm1639_result_t tm1639_display_on(output_driver_t *config);
static tm1639_result_t tm1639_display_off(output_driver_t *config);

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
	// 1. Standard Patterns (Segments a-g, dp) - Active High (1=ON)
	// We use standard patterns here because we will transpose them manually.
	static const uint8_t standard_patterns[16] = {
		0x3F, 0x06, 0x5B, 0x4F, // 0, 1, 2, 3
		0x66, 0x6D, 0x7D, 0x07, // 4, 5, 6, 7
		0x7F, 0x6F, 0x77, 0x7C, // 8, 9, A, b
		0x39, 0x5E, 0x79, 0x71 // C, d, E, F
	};

	// 2. Clear the buffer (Active Low logic usually requires 0s, but here
	//    the 'Data' selects the DIGIT. 0 = Digit OFF, 1 = Digit ON).
	//    Let's initialize to 0x00 (All digits OFF).
	for (uint8_t k = 0; k < TM1639_DISPLAY_BUFFER_SIZE; k++) {
		config->prep_buffer[k] = 0x00;
	}

	tm1639_result_t result = TM1639_OK;

	// 3. Transpose Logic: Loop through each DIGIT
	for (uint8_t digit_idx = 0; digit_idx < TM1639_DIGIT_COUNT; digit_idx++)
	{
		uint8_t bcd = digits[digit_idx] & 0x0F;
		uint8_t pattern = standard_patterns[bcd];

		// Add Decimal Point if needed (Standard Pattern uses Bit 7 for DP)
		if (digit_idx == dot_position)
		{
			pattern |= 0x80;
		}

		// 4. Distribute the pattern bits to the Segment Addresses
		// The TM1638 maps standard bits (0-7) to Segments (A-DP).
		// On your board, Addresses 0x00-0x0E correspond to these Segments.

		for (uint8_t seg_bit = 0; seg_bit < 8; seg_bit++)
		{
			// Check if this segment (A, B, C...) is ON for this digit
			if ((pattern >> seg_bit) & 0x01)
			{
				// Calculate Address for this Segment
				// Seg A (Bit 0) -> Addr 0x00
				// Seg B (Bit 1) -> Addr 0x02 ...
				uint8_t seg_addr = seg_bit * 2;

				// Determine which Bit represents this DIGIT
				// Since Pico is MSB-First and TM1638 is LSB-First:
				// Digit 1 (Index 0) -> TM1638 SEG1 (Bit 0) -> Pico sends Bit 7 (0x80)
				// Digit 2 (Index 1) -> TM1638 SEG2 (Bit 1) -> Pico sends Bit 6 (0x40)
				uint8_t digit_mask = (0x80 >> digit_idx);

				// Write the Digit Mask to the Segment Address
				if (seg_addr < TM1639_DISPLAY_BUFFER_SIZE)
				{
					config->prep_buffer[seg_addr] |= digit_mask;
				}
			}
		}
	}

	// 5. Fill Odd Addresses with 0x00 (Unused in this mode)
	for (uint8_t k = 1; k < TM1639_DISPLAY_BUFFER_SIZE; k += 2) {
		config->prep_buffer[k] = 0x00;
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
		config->set_brightness = &tm1639_set_brightness;

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

		// Test (set digits)
		if (TM1639_OK != tm1639_set_digits(config, (const uint8_t[]){0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, 8, TM1639_NO_DECIMAL_POINT))
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

static tm1639_result_t tm1639_display_on(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = true;
		result = tm1639_send_command(config, (uint8_t)TM1639_CMD_DISPLAY_ON);     // DISPLAY_ON | brightness (bit-reversed)
	}
	return result;
}

static tm1639_result_t tm1639_display_off(output_driver_t *config)
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

output_result_t tm1639_set_brightness(output_driver_t *config, uint8_t level)
{
	tm1639_result_t result = TM1639_OK;
	if (NULL == config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		uint8_t set_level = (level > (uint8_t)8) ? (uint8_t)8 : level;
		config->brightness = set_level;
		if (set_level > 0U)
		{
			set_level -= 1;
			config->display_on = true;
			// Display control command with brightness level (MSB-first SPI, bit-reversed command)
			(void)tm1639_display_on(config);
			uint8_t reversed_level = (uint8_t)(((set_level & 0x01U) << 2U) | (set_level & 0x02U) | ((set_level & 0x04U) >> 2U));
			uint8_t cmd = (uint8_t)TM1639_CMD_DISPLAY_ON | (uint8_t)(reversed_level << 5U);
			result = tm1639_send_command(config, cmd);
		}
		else
		{
			config->display_on = false;
			(void)tm1639_display_off(config);
		}
	}

	return tm1639_to_output_result(result);
}

tm1639_result_t tm1639_clear(output_driver_t *config)
{
	tm1639_result_t result = TM1639_OK;

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

		// Auto-increment mode, write data: 01000000 (0x40) - Invert for MSB first SPI
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
