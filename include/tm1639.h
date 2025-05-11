/**
 * @file tm1639.h
 * @brief TM1639 LED Driver Header for Raspberry Pi Pico using SPI hardware
 *
 * This header provides an interface for controlling TM1639 LED driver chips
 * using the Raspberry Pi Pico's hardware SPI interface and chip select through
 * a 74HC138 multiplexer.
 */

 #ifndef TM1639_H
 #define TM1639_H

 #include <stdint.h>
 #include <stdbool.h>
 #include "pico/stdlib.h"
 #include "hardware/spi.h"
 #include "FreeRTOS.h"
 #include "semphr.h"

// TM1639 Command Constants
 #define TM1639_CMD_DATA_WRITE      0x40  // Write data to display register
 #define TM1639_CMD_DATA_READ_KEYS  0x42  // Read key scanning data
 #define TM1639_CMD_FIXED_ADDR      0x44  // Fixed address mode
 #define TM1639_CMD_DISPLAY_OFF     0x80  // Display off
 #define TM1639_CMD_DISPLAY_ON      0x88  // Display on
 #define TM1639_CMD_ADDR_BASE       0xC0  // Base address command

// Error codes
 #define TM1639_OK                  0     // Operation successful
 #define TM1639_ERR_SPI_INIT       -1     // SPI initialization error
 #define TM1639_ERR_GPIO_INIT      -2     // GPIO initialization error
 #define TM1639_ERR_SPI_WRITE      -3     // SPI write error
 #define TM1639_ERR_INVALID_PARAM  -4     // Invalid parameter
 #define TM1639_ERR_ADDRESS_RANGE  -5     // Address out of range
 #define TM1639_ERR_MUTEX          -6     // Mutex error
 #define TM1639_ERR_MUTEX_TIMEOUT  -7     // Mutex acquisition timeout

// Structure for pressed key information
typedef struct {
	uint8_t ks; // Key scan line (1-4)
	uint8_t k; // Key input line (0-1)
} tm1639_key_t;

// TM1639 configuration structure
typedef struct {
	// Chip id
	uint8_t chip_id;

	// Add the function pointer to handle chip selection.
	// The function should accept a chip id and a boolean to indicate selection (true = select, false = deselect)
	void (*select_interface)(uint8_t chip_id, bool select);

	// Mutex and semaphores
	SemaphoreHandle_t spi_mutex; // Mutex for SPI bus access
	TickType_t mutex_timeout;  // Timeout for mutex acquisition (in ticks)

	// Display state
	uint8_t active_buffer[16]; // Active display buffer (16 bytes)
	uint8_t prep_buffer[16];   // Preparation buffer for double buffering
	bool buffer_modified;      // Flag to track if prep buffer has been modified
	uint8_t brightness;        // Current brightness level (0-7)
	bool display_on;           // Display on/off state
} tm1639_t;

// External definition of digit patterns
extern const uint8_t tm1639_digit_patterns[16];

/**
 * @brief Initialize the TM1639 driver
 *
 * This function allocates and initializes a new TM1639 configuration structure.
 * Each device can have its own configuration including buffers.
 *
 * @param chip_id Identifier for the specific TM1639 chip
 * @param select_interface Function to handle chip selection
 * @param spi SPI instance to use
 * @param dio_pin Data I/O pin
 * @param clk_pin Clock pin
 * @param spi_mutex Existing mutex for SPI bus access (pass NULL to not use mutex)
 * @param mutex_timeout Timeout for mutex acquisition in ticks (use portMAX_DELAY for no timeout)
 * @return Pointer to the initialized TM1639 configuration structure, or NULL if allocation failed
 */
tm1639_t* tm1639_init(uint8_t chip_id,
                      void (*select_interface)(uint8_t chip_id, bool select),
                      spi_inst_t *spi,
                      uint8_t dio_pin,
                      uint8_t clk_pin,
                      SemaphoreHandle_t spi_mutex,
                      TickType_t mutex_timeout);

/**
 * @brief Send a command to the TM1639
 *
 * @param config Pointer to TM1639 configuration structure
 * @param cmd Command byte to send
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_send_command(tm1639_t *config, uint8_t cmd);

/**
 * @brief Set the display memory address (0x00-0x0F)
 *
 * @param config Pointer to TM1639 configuration structure
 * @param addr Address to set (0-15)
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_address(tm1639_t *config, uint8_t addr);

/**
 * @brief Set the data command mode
 *
 * @param config Pointer to TM1639 configuration structure
 * @param cmd Data command byte
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_data_command(tm1639_t *config, uint8_t cmd);

/**
 * @brief Write data to a specific address
 *
 * @param config Pointer to TM1639 configuration structure
 * @param addr Address to write to (0-15)
 * @param data Data byte to write
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_write_data_at(tm1639_t *config, uint8_t addr, uint8_t data);

/**
 * @brief Write multiple bytes starting at the given address
 *
 * @param config Pointer to TM1639 configuration structure
 * @param addr Starting address (0-15)
 * @param data_bytes Pointer to data bytes to write
 * @param length Number of bytes to write
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_write_data(tm1639_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length);

/**
 * @brief Read the key scan data
 *
 * @param config Pointer to TM1639 configuration structure
 * @param key_data Pointer to 2-byte array to store key data
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_read_keys(tm1639_t *config, uint8_t *key_data);

/**
 * @brief Read key states and return number of pressed keys
 *
 * @param config Pointer to TM1639 configuration structure
 * @param keys Array to store pressed key information (at least 8 elements)
 * @return int Number of pressed keys detected, or negative error code
 */
uint8_t tm1639_get_key_states(tm1639_t *config, tm1639_key_t *keys);

/**
 * @brief Set the display brightness level (0-7)
 *
 * @param config Pointer to TM1639 configuration structure
 * @param level Brightness level (0-7)
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_brightness(tm1639_t *config, uint8_t level);

/**
 * @brief Turn the display on
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_display_on(tm1639_t *config);

/**
 * @brief Turn the display off
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_display_off(tm1639_t *config);

/**
 * @brief Clear the display (set all segments off)
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_clear(tm1639_t *config);

/**
 * @brief Update the display with the current preparation buffer contents
 *
 * This function updates the display with the contents of the prep buffer
 * and swaps the active and prep buffers. Only updates if buffer_modified is true.
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_update(tm1639_t *config);

/**
 * @brief Set segments for a specific position (for 7-segment displays)
 *
 * @param config Pointer to TM1639 configuration structure
 * @param position Digit position (0-7)
 * @param segments Segment pattern (bits represent segments a-g)
 * @param dp Decimal point state (true/false)
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_segment(tm1639_t *config, uint8_t position, uint8_t segments, bool dp);

/**
 * @brief Set an entire row in matrix mode
 *
 * @param config Pointer to TM1639 configuration structure
 * @param row Row index (0-7)
 * @param data 8-bit data for the row
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_matrix_row(tm1639_t *config, uint8_t row, uint8_t data);

/**
 * @brief Display a hexadecimal digit (0-F) at the specified position
 *
 * @param config Pointer to TM1639 configuration structure
 * @param position Digit position (0-7)
 * @param digit Digit to display (0-15)
 * @param dp Decimal point state (true/false)
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_set_digit(tm1639_t *config, uint8_t position, uint8_t digit, bool dp);

/**
 * @brief Deinitialize the TM1639 driver and release resources
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_deinit(tm1639_t *config);

/**
 * @brief Updates a specific position in the preparation buffer without immediately displaying
 *
 * @param config Pointer to TM1639 configuration structure
 * @param addr Address to modify (0-15)
 * @param data New data
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_update_buffer(tm1639_t *config, uint8_t addr, uint8_t data);

/**
 * @brief Immediately display the preparation buffer without waiting for update
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
uint8_t tm1639_flush(tm1639_t *config);

 #endif /* TM1639_H */