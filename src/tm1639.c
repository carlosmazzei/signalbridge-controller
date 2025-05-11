/**
 * @file tm1639.c
 * @brief Implementation of the TM1639 LED driver using SPI hardware for Raspberry Pi Pico
 *
 * This driver utilizes the Raspberry Pi Pico's SPI0 hardware interface to control
 * the TM1639 LED driver chip
 */

 #include <stdio.h>
 #include <string.h>

 #include "pico/stdlib.h"
 #include "hardware/spi.h"
 #include "hardware/gpio.h"
 #include "hardware/clocks.h"
 #include "hardware/pio.h"
 #include "FreeRTOS.h"
 #include "semphr.h"

 #include "tm1639.h"

// Segment patterns for hex digits 0-F
// Bits: dp-g-f-e-d-c-b-a (MSB to LSB)
const uint8_t tm1639_digit_patterns[] = {
	0x3F, 0x06, 0x5B, 0x4F, // 0, 1, 2, 3
	0x66, 0x6D, 0x7D, 0x07, // 4, 5, 6, 7
	0x7F, 0x6F, 0x77, 0x7C, // 8, 9, A, b
	0x39, 0x5E, 0x79, 0x71 // C, d, E, F
};

/**
 * @brief Initialize the TM1639 driver
 */
tm1639_t* tm1639_init(uint8_t chip_id,
                      void (*select_interface)(uint8_t chip_id, bool select),
                      spi_inst_t *spi,
                      uint8_t dio_pin,
                      uint8_t clk_pin,
                      SemaphoreHandle_t spi_mutex,
                      TickType_t mutex_timeout)
{
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
		return clear_result;
	}

	// Set maximum brightness
	int8_t brightness_result = tm1639_set_brightness(config, 7);
	if (brightness_result != TM1639_OK)
	{
		return brightness_result;
	}

	// Turn on display
	int8_t display_result = tm1639_display_on(config);
	if (display_result != TM1639_OK)
	{
		return display_result;
	}

	return TM1639_OK;
}

/**
 * @brief Start a transmission by setting STB low via multiplexer
 */
static void tm1639_start(const tm1639_t *config)
{
	config->select_interface(config->chip_id, true); // Select the chip (STB low)
}

/**
 * @brief End a transmission by setting STB high via multiplexer
 */
static void tm1639_stop(const tm1639_t *config)
{
	config->select_interface(config->chip_id, false); // Deselect the chip (STB high)
}

/**
 * @brief Acquire the SPI mutex with timeout
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
static int8_t tm1639_acquire_mutex(tm1639_t *config)
{
	if (!config->spi_mutex) {
		return TM1639_OK; // No mutex to acquire
	}

	if (pdTRUE == xSemaphoreTake(config->spi_mutex, config->mutex_timeout)) {
		return TM1639_OK;
	} else {
		return TM1639_ERR_MUTEX_TIMEOUT;
	}
}

/**
 * @brief Release the SPI mutex
 *
 * @param config Pointer to TM1639 configuration structure
 */
static void tm1639_release_mutex(const tm1639_t *config)
{
	if (config->spi_mutex) {
		xSemaphoreGive(config->spi_mutex);
	}
}

/**
 * @brief Write a byte to the TM1639 using SPI
 *
 * @return int Error code, 0 if successful
 */
static int8_t tm1639_write_byte(const tm1639_t *config, uint8_t data)
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
static void tm1639_set_read_mode(const tm1639_t *config)
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
static void tm1639_set_write_mode(const tm1639_t *config)
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
static int8_t tm1639_read_bytes(const tm1639_t *config, uint8_t *data, uint8_t count)
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
int8_t tm1639_send_command(tm1639_t *config, uint8_t cmd)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Acquire mutex
	int8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK)
	{
		return mutex_result;
	}

	tm1639_start(config);
	int8_t result = tm1639_write_byte(config, cmd);
	tm1639_stop(config);

	// Release mutex
	tm1639_release_mutex(config);

	return result;
}

/**
 * @brief Set the display memory address (0x00-0x0F)
 */
int8_t tm1639_set_address(tm1639_t *config, uint8_t addr)
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
int8_t tm1639_set_data_command(tm1639_t *config, uint8_t cmd)
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
int8_t tm1639_write_data_at(tm1639_t *config, uint8_t addr, uint8_t data)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Acquire mutex
	int8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK)
	{
		return mutex_result;
	}

	tm1639_start(config);
	// Set fixed address mode
	int8_t result = tm1639_write_byte(config, 0x44); // DATA_CMD_FIXED_ADDR
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Set the address
	result = tm1639_write_byte(config, 0xC0 | (addr & 0x0F));
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Write the data
	result = tm1639_write_byte(config, data);
	tm1639_stop(config);

	// Release mutex
	tm1639_release_mutex(config);

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
uint8_t tm1639_update_buffer(tm1639_t *config, uint8_t addr, uint8_t data)
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
int8_t tm1639_write_data(tm1639_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length)
{
	if (!config || !data_bytes)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	if (addr > 0x0F)
	{
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Acquire mutex
	int8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK)
	{
		return mutex_result;
	}

	tm1639_start(config);
	// Set auto-increment mode
	int8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Set the starting address
	result = tm1639_write_byte(config, 0xC0 | (addr & 0x0F));
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		tm1639_release_mutex(config);
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
				tm1639_release_mutex(config);
				return result;
			}

			config->active_buffer[addr + i] = data_bytes[i];
		}
	}

	tm1639_stop(config);
	tm1639_release_mutex(config);
	return TM1639_OK;
}

/**
 * @brief Read the key scan data
 */
int8_t tm1639_read_keys(tm1639_t *config, uint8_t *key_data)
{
	if (!config || !key_data)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Acquire mutex
	int8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK)
	{
		return mutex_result;
	}

	tm1639_start(config);
	// Set key reading mode
	int8_t result = tm1639_write_byte(config, 0x42); // DATA_CMD_READ_KEYS
	if (result != TM1639_OK)
	{
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Read key scan data (2 bytes)
	result = tm1639_read_bytes(config, key_data, 2);
	tm1639_stop(config);

	tm1639_release_mutex(config);
	return result;
}

/**
 * @brief Read key states and return number of pressed keys
 */
int8_t tm1639_get_key_states(tm1639_t *config, tm1639_key_t *keys)
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
uint8_t tm1639_set_brightness(tm1639_t *config, uint8_t level)
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
uint8_t tm1639_display_on(tm1639_t *config)
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
uint8_t tm1639_display_off(tm1639_t *config)
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
uint8_t tm1639_clear(tm1639_t *config)
{
	if (!config)
	{
		return TM1639_ERR_INVALID_PARAM;
	}

	// Fill buffer with zeros
	memset(config->active_buffer, 0, sizeof(config->active_buffer));
	memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
	config->buffer_modified = false;

	// Acquire mutex
	uint8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK) {
		return mutex_result;
	}

	// Write zeros to all addresses
	tm1639_start(config);
	uint8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK) {
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	result = tm1639_write_byte(config, 0xC0); // Start address 0
	if (result != TM1639_OK) {
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Write 16 bytes of 0
	for (uint8_t i = 0; i < 16; i++) {
		result = tm1639_write_byte(config, 0);
		if (result != TM1639_OK) {
			tm1639_stop(config);
			tm1639_release_mutex(config);
			return result;
		}
	}

	tm1639_stop(config);
	tm1639_release_mutex(config);
	return TM1639_OK;
}

/**
 * @brief Flush the preparation buffer to the display immediately
 */
uint8_t tm1639_flush(tm1639_t *config) {
	if (!config) {
		return TM1639_ERR_INVALID_PARAM;
	}

	// Copy prep buffer to active buffer
	memcpy(config->active_buffer, config->prep_buffer, sizeof(config->active_buffer));

	// Reset the modified flag
	config->buffer_modified = false;

	// Acquire mutex
	uint8_t mutex_result = tm1639_acquire_mutex(config);
	if (mutex_result != TM1639_OK) {
		return mutex_result;
	}

	// Write to the display
	tm1639_start(config);
	uint8_t result = tm1639_write_byte(config, 0x40); // DATA_CMD_AUTO_INC
	if (result != TM1639_OK) {
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	result = tm1639_write_byte(config, 0xC0); // Start address 0
	if (result != TM1639_OK) {
		tm1639_stop(config);
		tm1639_release_mutex(config);
		return result;
	}

	// Write all 16 bytes from buffer
	for (uint8_t i = 0; i < 16; i++) {
		result = tm1639_write_byte(config, config->active_buffer[i]);
		if (result != TM1639_OK) {
			tm1639_stop(config);
			tm1639_release_mutex(config);
			return result;
		}
	}

	tm1639_stop(config);
	tm1639_release_mutex(config);
	return TM1639_OK;
}

/**
 * @brief Update the display with the current buffer contents
 */
uint8_t tm1639_update(tm1639_t *config) {
	if (!config) {
		return TM1639_ERR_INVALID_PARAM;
	}

	// Only update if buffer has been modified
	if (!config->buffer_modified) {
		return TM1639_OK;
	}

	return tm1639_flush(config);
}

/**
 * @brief Set segments for a specific position (for 7-segment displays)
 */
uint8_t tm1639_set_segment(tm1639_t *config, uint8_t position, uint8_t segments, bool dp) {
	if (!config) {
		return TM1639_ERR_INVALID_PARAM;
	}

	if (position >= 8) { // Max 8 positions
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Calculate address based on position
	uint8_t addr = position * 2; // Each position uses 2 bytes in memory

	// Set segments with decimal point if needed
	uint8_t data = segments;
	if (dp) {
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
uint8_t tm1639_set_matrix_row(tm1639_t *config, uint8_t row, uint8_t data) {
	if (!config) {
		return TM1639_ERR_INVALID_PARAM;
	}

	if (row >= 8) { // Max 8 rows
		return TM1639_ERR_ADDRESS_RANGE;
	}

	// Calculate address for this row
	uint8_t addr = row * 2;

	// Update buffer and mark for update
	config->prep_buffer[addr] = data;
	config->buffer_modified = true;

	return TM1639_OK;
}

/**
 * @brief Display a hexadecimal digit (0-F) at the specified position
 */
uint8_t tm1639_set_digit(tm1639_t *config, uint8_t position, uint8_t digit, bool dp) {
	if (!config) {
		return TM1639_ERR_INVALID_PARAM;
	}

	if (digit > 15) {
		digit = 0; // Default to 0 if out of range
	}

	// Set the segments using the pattern for this digit
	return tm1639_set_segment(config, position, tm1639_digit_patterns[digit], dp);
}

/**
 * @brief Deinitialize the TM1639 driver and release resources
 */
uint8_t tm1639_deinit(tm1639_t *config)
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