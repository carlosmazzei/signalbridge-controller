/**
 * @file tm1639.c
 * @brief Implementation of the TM1639 LED driver using SPI hardware for Raspberry Pi Pico
 *
 * This driver utilizes the Raspberry Pi Pico's SPI0 hardware interface to control
 * the TM1639 LED driver chip
 */

 #include <string.h>

 #include "pico/stdlib.h"
 #include "hardware/spi.h"
 #include "hardware/gpio.h"
 #include "hardware/clocks.h"
 #include "hardware/pio.h"
 #include "FreeRTOS.h"
 #include "semphr.h"

 #include "tm1639.h"
 #include "outputs.h"

/**
 * @brief Initialize the TM1639 driver
 */
output_driver_t* tm1639_init(uint8_t chip_id,
                             uint8_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin)
{
	output_driver_t *config = pvPortMalloc(sizeof(output_driver_t));
	if (!config)
	{
		return NULL;
	}

	// Check if all the parameters are valid
	if (config->chip_id >= MAX_SPI_INTERFACES || !config->select_interface ||
	    !config->spi)
	{
		vPortFree(config);
		return NULL;
	}

	// Check if the GPIO pins are valid
	if (config->dio_pin >= NUM_GPIO || config->clk_pin >= NUM_GPIO)
	{
		vPortFree(config);
		return NULL;
	}

	// Initialize GPIO pins and SPI interface
	config->chip_id = chip_id;
	config->select_interface = select_interface;
	config->spi = spi;
	config->dio_pin = dio_pin;
	config->clk_pin = clk_pin;

	// Initialize buffer and state
	memset(config->active_buffer, 0, sizeof(config->active_buffer));
	memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
	config->buffer_modified = false;
	config->brightness = 7;
	config->display_on = false;

	// Clear display on startup
	int8_t clear_result = tm1639_clear(config);
	if (clear_result != TM1639_OK)
	{
		vPortFree(config);
		return NULL;
	}

	// Set maximum brightness
	int8_t brightness_result = tm1639_set_brightness(config, 7);
	if (brightness_result != TM1639_OK)
	{
		vPortFree(config);
		return NULL;
	}

	// Turn on display
	int8_t display_result = tm1639_display_on(config);
	if (display_result != TM1639_OK)
	{
		vPortFree(config);
		return NULL;
	}

	return config;
}

/**
 * @brief Start a transmission by setting STB low via multiplexer
 */
static void tm1639_start(const output_driver_t *config)
{
	config->select_interface(config->chip_id, true); // Select the chip (STB low)
}

/**
 * @brief End a transmission by setting STB high via multiplexer
 */
static void tm1639_stop(const output_driver_t *config)
{
	config->select_interface(config->chip_id, false); // Deselect the chip (STB high)
}

/**
 * @brief Write a byte to the TM1639 using SPI
 *
 * @return int Error code, 0 if successful
 */
static int8_t tm1639_write_byte(const output_driver_t *config, uint8_t data)
{
	// Use SPI hardware to write the byte
	int bytes_written = spi_write_blocking(config->spi, &data, 1);
	if (bytes_written != 1)
	{
		return TM1639_ERR_SPI_WRITE;
	}
	return TM1639_OK;
}

/**
 * @brief Configure GPIO for reading from DIO pin
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
 * @brief Configure GPIO back to SPI mode for writing
 */
static void tm1639_set_write_mode(const output_driver_t *config)
{
	// Restore SPI function for DIO pin
	gpio_set_function(config->dio_pin, GPIO_FUNC_SPI);
	gpio_set_function(config->clk_pin, GPIO_FUNC_SPI);
}

/**
 * @brief Bit-bang read operation for TM1639
 *
 * Since SPI hardware typically doesn't allow mid-transaction direction change,
 * we need to bit-bang the read operation.
 */
static int8_t tm1639_read_bytes(const output_driver_t *config, uint8_t *data, uint8_t count)
{
	if (!config || !data)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Switch DIO to input mode
	tm1639_set_read_mode(config);

	// Waiting time (Twait) required by TM1639 protocol - at least 2µs
	sleep_us(2);

	// Manual bit-banging to read data
	for (uint8_t i = 0; i < count; i++)
	{
		data[i] = 0;

		// Manual bit-banging of clock to read 8 bits
		for (uint8_t bit = 0; bit < 8; bit++)
		{
			// Set clock high
			gpio_put(config->clk_pin, 1);
			sleep_us(1); // Brief delay

			// Read data bit (LSB first)
			if (gpio_get(config->dio_pin))
			{
				data[i] |= (1 << bit);
			}

			// Set clock low
			gpio_put(config->clk_pin, 0);
			sleep_us(1); // Brief delay
		}
	}

	// Restore pins to SPI mode
	tm1639_set_write_mode(config);

	return TM1639_OK;
}

/**
 * @brief Send a command to the TM1639
 */
int8_t tm1639_send_command(output_driver_t *config, uint8_t cmd)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	tm1639_start(config);
	int8_t result = tm1639_write_byte(config, cmd);
	tm1639_stop(config);

	return result;
}

/**
 * @brief Set the display memory address (0x00-0x0F)
 */
int8_t tm1639_set_address(output_driver_t *config, uint8_t addr)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Address command: B7:B6 = 11, Address in B3:B0
	uint8_t cmd = 0xC0 | (addr & 0x0F);
	return tm1639_send_command(config, cmd);
}

/**
 * @brief Set the data command mode
 */
int8_t tm1639_set_data_command(output_driver_t *config, uint8_t cmd)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Data command: B7:B6 = 01, followed by specific command bits
	return tm1639_send_command(config, cmd);
}

/**
 * @brief Write data to a specific address
 */
int8_t tm1639_write_data_at(output_driver_t *config, uint8_t addr, uint8_t data)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	tm1639_start(config);
	// Set fixed address mode
	int8_t result = tm1639_write_byte(config, 0x44); // DATA_CMD_FIXED_ADDR
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Set the address
	result = tm1639_write_byte(config, 0xC0 | (addr & 0x0F));
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Write the data
	result = tm1639_write_byte(config, data);
	tm1639_stop(config);

	if (TM1639_OK == result)
	{
		// Update active buffer
		config->active_buffer[addr] = data;
	}

	return result;
}

/**
 * @brief Update specific bytes in the preparation buffer
 */
int8_t tm1639_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Update prep buffer and mark as modified
	config->prep_buffer[addr] = data;
	config->buffer_modified = true;

	return TM1639_OK;
}

/**
 * @brief Write multiple bytes starting at the given address
 */
int8_t tm1639_write_data(output_driver_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length)
{
	if (!config || !data_bytes)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	tm1639_start(config);
	// Set auto-increment mode
	int8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Set the starting address
	result = tm1639_write_byte(config, 0xC0 | (addr & 0x0F));
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Write all data bytes
	for (uint8_t i = 0; i < length; i++)
	{
		if ((addr + i) < 16)
		{ // Ensure we don't exceed address space
			result = tm1639_write_byte(config, data_bytes[i]);
			if (result != TM1639_OK)
			{
				tm1639_stop(config);
				return result;
			}

			config->active_buffer[addr + i] = data_bytes[i];
		}
	}

	tm1639_stop(config);
	return TM1639_OK;
}

/**
 * @brief Read the key scan data
 */
int8_t tm1639_read_keys(output_driver_t *config, uint8_t *key_data)
{
	if (!config || !key_data)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	tm1639_start(config);
	// Set key reading mode
	int8_t result = tm1639_write_byte(config, 0x42); // DATA_CMD_READ_KEYS
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Read key scan data (2 bytes)
	result = tm1639_read_bytes(config, key_data, 2);
	tm1639_stop(config);

	return result;
}

/**
 * @brief Read key states and return number of pressed keys
 */
int8_t tm1639_get_key_states(output_driver_t *config, tm1639_key_t *keys)
{
	if (!config || !keys)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	uint8_t key_data[2];
	uint8_t result = tm1639_read_keys(config, key_data);
	if (result != TM1639_OK)
	{
		return result;
	}

	uint8_t count = 0;

	// Process the two key data bytes
	// BYTE1 contains K0-K1 status for KS1 and KS2
	// BYTE2 contains K0-K1 status for KS3 and KS4
	for (uint8_t ks_idx = 0; ks_idx < 2; ks_idx++)
	{
		uint8_t byte = key_data[ks_idx];
		uint8_t ks_base = ks_idx * 2 + 1; // KS1/KS2 for first byte, KS3/KS4 for second

		// Check K0 and K1 bits (bits 2-3 for K0, bits 6-7 for K1)
		if (byte & 0x04)
		{ // K0 for first KS in this byte
			keys[count].ks = ks_base;
			keys[count].k = 0;
			count++;
		}
		if (byte & 0x08)
		{ // K1 for first KS in this byte
			keys[count].ks = ks_base;
			keys[count].k = 1;
			count++;
		}
		if (byte & 0x40)
		{ // K0 for second KS in this byte
			keys[count].ks = ks_base + 1;
			keys[count].k = 0;
			count++;
		}
		if (byte & 0x80)
		{ // K1 for second KS in this byte
			keys[count].ks = ks_base + 1;
			keys[count].k = 1;
			count++;
		}
	}

	return count;
}

/**
 * @brief Set the display brightness level (0-7)
 */
int8_t tm1639_set_brightness(output_driver_t *config, uint8_t level)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (level > 7)
	{
		level = 7; // Clamp to valid range
	}

	config->brightness = level;

	// Display control command with brightness level
	uint8_t cmd = 0x88 | level; // DISPLAY_ON | level
	return tm1639_send_command(config, cmd);
}

/**
 * @brief Turn the display on
 */
int8_t tm1639_display_on(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	config->display_on = true;
	return tm1639_send_command(config, 0x88 | config->brightness); // DISPLAY_ON | brightness
}

/**
 * @brief Turn the display off
 */
int8_t tm1639_display_off(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	config->display_on = false;
	return tm1639_send_command(config, 0x80); // DISPLAY_OFF
}

/**
 * @brief Clear the display (set all segments off)
 */
int8_t tm1639_clear(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Fill buffer with zeros
	memset(config->active_buffer, 0, sizeof(config->active_buffer));
	memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
	config->buffer_modified = false;

	// Write zeros to all addresses
	tm1639_start(config);
	uint8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	result = tm1639_write_byte(config, 0xC0); // Start address 0
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Write 16 bytes of 0
	for (uint8_t i = 0; i < 16; i++) {
		result = tm1639_write_byte(config, 0);
		if (result != TM1639_OK)
		{
			tm1639_stop(config);
			return result;
		}
	}

	tm1639_stop(config);
	return TM1639_OK;
}

/**
 * @brief Flush the preparation buffer to the display immediately
 */
int8_t tm1639_flush(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Copy prep buffer to active buffer
	memcpy(config->active_buffer, config->prep_buffer, sizeof(config->active_buffer));

	// Reset the modified flag
	config->buffer_modified = false;

	// Write to the display
	tm1639_start(config);
	uint8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Write to the display
	tm1639_start(config);
	result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	result = tm1639_write_byte(config, 0xC0); // Start address 0
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		return result;
	}

	// Write all 16 bytes from buffer
	for (uint8_t i = 0; i < 16; i++) {
		result = tm1639_write_byte(config, config->active_buffer[i]);
		if (result != TM1639_OK)
		{
			tm1639_stop(config);
			return result;
		}
	}

	tm1639_stop(config);
	return TM1639_OK;
}

/**
 * @brief Update the display with the current buffer contents
 */
int8_t tm1639_update(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Only update if buffer has been modified
	if (!config->buffer_modified)
	{
		return TM1639_OK;
	}

	return tm1639_flush(config);
}

int8_t tm1639_set_segment(output_driver_t *config, uint8_t position, uint8_t segments, bool dp)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (position >= 8)   // Max 8 positions
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Calculate address based on position
	uint8_t addr = position * 2; // Each position uses 2 bytes in memory

	// Set segments with decimal point if needed
	uint8_t data = segments;
	if (dp)
	{
		data |= 0x80; // Set MSB for decimal point
	}

	// Update buffer and mark for update
	config->prep_buffer[addr] = data;
	config->buffer_modified = true;

	return TM1639_OK;
}

/**
 * @brief Set an entire row in matrix mode
 */
int8_t tm1639_set_matrix_row(output_driver_t *config, uint8_t row, uint8_t data)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (row >= 8)   // Max 8 rows
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Calculate address for this row
	uint8_t addr = row * 2;

	// Update buffer and mark for update
	config->prep_buffer[addr] = data;
	config->buffer_modified = true;

	return TM1639_OK;
}

uint8_t tm1639_set_digits(output_driver_t *config,
                          const uint8_t* digits,
                          const size_t length,
                          const uint8_t dot_position)
{
	// Segment patterns for hex digits 0-F
	// Bits: dp-g-f-e-d-c-b-a (MSB to LSB)
	static const uint8_t tm1639_digit_patterns[16] = {
		0x3F, 0x06, 0x5B, 0x4F, // 0, 1, 2, 3
		0x66, 0x6D, 0x7D, 0x07, // 4, 5, 6, 7
		0x7F, 0x6F, 0x77, 0x7C, // 8, 9, A, b
		0x39, 0x5E, 0x79, 0x71 // C, d, E, F
	};

	uint8_t result;
	if (!config)
	{
		result = TM1639_ERR_INVALID_PARAM;
	}

	if ((digits == NULL) ||
	    (dot_position > (uint8_t)7) ||
	    (length < (uint8_t)8)) // Ensure length is at least 8
	{
		// Ensure digit array is valid and has 8 elements
		result = TM1639_ERR_INVALID_PARAM;
	}
	else
	{
		for (uint8_t i = (uint8_t)0; i < (uint8_t)8; i++)
		{
			// Set each digit segment
			bool dp = (i == dot_position); // Set decimal point only for specified position
			result = tm1639_set_segment(config, i, tm1639_digit_patterns[digits[i]], dp);
		}
		/* Update the display */
		result = tm1639_update(config);
	}

	return result;
}

/**
 * @brief Deinitialize the TM1639 driver and release resources
 */
int8_t tm1639_deinit(output_driver_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Turn off display
	int8_t result = tm1639_display_off(config);
	if (result != TM1639_OK)
	{
		return result;
	}

	return TM1639_OK;
}