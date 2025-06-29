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
#include "outputs.h"

// TM1639 Command Constants
#define TM1639_CMD_DATA_WRITE      0x40  // Write data to display register
#define TM1639_CMD_DATA_READ_KEYS  0x42  // Read key scanning data
#define TM1639_CMD_FIXED_ADDR      0x44  // Fixed address mode
#define TM1639_CMD_DISPLAY_OFF     0x80  // Display off
#define TM1639_CMD_DISPLAY_ON      0x88  // Display on
#define TM1639_CMD_ADDR_BASE       0xC0  // Base address command

#define TM1639_MAX_DISPLAY_REGISTERS    (16U)
#define TM1639_DIGIT_COUNT              (8U)    /* Number of digits supported */
#define TM1639_DISPLAY_BUFFER_SIZE      (16U)   /* Total display buffer size */
#define TM1639_DECIMAL_POINT_MASK       (0x80U) /* Mask for decimal point bit */
#define TM1639_NO_DECIMAL_POINT         (0xFFU) /* Value indicating no decimal point */
#define TM1639_BCD_MASK                 (0x0FU) /* Mask for BCD digit */
#define TM1639_BCD_MAX_VALUE            (9U)     /* Maximum valid BCD digit */
#define TM1639_ERR_INVALID_BCD          (5U)    /* Error code for invalid BCD */

/**
 * @brief TM1639 Result Codes
 * This enum defines the possible result codes returned by TM1639 functions.
 * Each code indicates the success or type of error encountered during operations.
 */
typedef enum tm1639_result_t {
	TM1639_OK = 0,
	TM1639_ERR_SPI_INIT = 1,
	TM1639_ERR_GPIO_INIT = 2,
	TM1639_ERR_SPI_WRITE = 3,
	TM1639_ERR_INVALID_PARAM = 4,
	TM1639_ERR_ADDRESS_RANGE = 5,
	TM1639_ERR_MUTEX_TIMEOUT = 6,
	TM1639_ERR_INVALID_CHAR = 7
} tm1639_result_t;

/**
 * @brief TM1639 Key Structure
 * This structure represents a key state in the TM1639 key scanning system.
 * It contains the key scan line and the key input line.
 * The key scan line indicates which of the 4 scan lines is being read,
 * and the key input line indicates which of the 2 key inputs is being read.
 */
typedef struct tm1639_key_t {
	uint8_t ks; // Key scan line (1-4)
	uint8_t k; // Key input line (0-1)
} tm1639_key_t;

/**
 * @brief Initialize the TM1639 driver.
 *
 * Allocates and initializes a TM1639 output driver structure.
 * Checks all parameters and hardware resources before returning a valid pointer.
 *
 * @param[in] chip_id           Controller ID (0-based).
 * @param[in] select_interface  Function pointer for chip selection.
 * @param[in] spi               SPI instance pointer.
 * @param[in] dio_pin           DIO GPIO pin number.
 * @param[in] clk_pin           CLK GPIO pin number.
 * @return Pointer to initialized output_driver_t structure, or NULL on error.
 */
output_driver_t* tm1639_init(uint8_t chip_id,
                             uint8_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin);

/**
 * @brief Send a command to the TM1639.
 *
 * This function sends a single command byte to the TM1639 device using the configured SPI hardware.
 *
 * @param[in] config Pointer to the TM1639 output driver configuration structure.
 * @param[in] cmd    Command byte to be sent to the TM1639.
 *
 * @return TM1639_OK on success, or TM1639_ERR_INVALID_PARAM if parameters are invalid.
 */
tm1639_result_t tm1639_send_command(const output_driver_t *config, uint8_t cmd);

/**
 * @brief Set the display memory address (0x00-0x0F)
 *
 * @param[in] config Pointer to TM1639 configuration structure
 * @param[in] addr Address to set (0-15)
 * @return tm1639_resul_t Result code, 0 if successful
 */
tm1639_result_t tm1639_set_address(const output_driver_t *config, uint8_t addr);

/**
 * @brief Write data to a specific address
 *
 * This function writes a single byte of data to a specific address in the TM1639 display memory.
 * It validates all parameters according to MISRA and SonarQube recommendations.
 *
 * @param[in, out]  config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[in]       addr   Address to write to (valid range: 0x00 to 0x0F).
 * @param[in]       data   Data byte to write.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         TM1639_ERR_ADDRESS_RANGE if addr is out of range,
 *         or error code from tm1639_write_byte.
 */
tm1639_result_t tm1639_write_data_at(output_driver_t *config, uint8_t addr, uint8_t data);

/**
 * @brief Write multiple bytes starting at the given address.
 *
 * This function writes multiple bytes to the TM1639 display memory, starting at the specified address.
 * All parameters are validated according to MISRA and SonarQube recommendations.
 * The function uses only one return statement.
 *
 * @param[in,out] config      Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[in]     addr        Start address to write to (valid range: 0x00 to 0x0F).
 * @param[in]     data_bytes  Pointer to the array of bytes to write. Must not be NULL.
 * @param[in]     length      Number of bytes to write. Must be > 0 and (addr + length) <= 16.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if any parameter is invalid,
 *         TM1639_ERR_ADDRESS_RANGE if address range is invalid,
 *         or error code from tm1639_write_byte.
 */
tm1639_result_t tm1639_write_data(output_driver_t *config, uint8_t addr, const uint8_t *data_bytes, uint8_t length);

/**
 * @brief Read key states.
 *
 * This function reads the key scan data from the TM1639 device and fills the provided
 * keys array with the detected key states. All parameters are validated according to
 * MISRA and SonarQube recommendations. Only one return point is used.
 *
 * @param[in]  config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[out] keys   Pointer to the array where detected key states will be stored. Must not be NULL.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if any parameter is invalid,
 *         or error code from tm1639_read_keys.
 */
tm1639_result_t tm1639_get_key_states(const output_driver_t *config, tm1639_key_t *keys);

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
tm1639_result_t tm1639_set_brightness(output_driver_t *config, uint8_t level);

/**
 * @brief Turn the display on.
 *
 * This function turns the TM1639 display on by setting the display_on flag in the driver
 * configuration and sending the display ON command with the current brightness level.
 *
 * @param[in,out] config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         or error code from tm1639_send_command.
 */
tm1639_result_t tm1639_display_on(output_driver_t *config);

/**
 * @brief Turn the display off
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
tm1639_result_t tm1639_display_off(output_driver_t *config);

/**
 * @brief Clear the display (set all segments off)
 *
 * This function clears the TM1639 display by setting all segment memory to zero.
 * It also clears the driver's active and preparation buffers and resets the modified flag.
 *
 * @param[in,out] config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         or error code from tm1639_write_byte.
 */
tm1639_result_t tm1639_clear(output_driver_t *config);

/**
 * @brief Set segments for display (for 7-segment displays)
 *
 * @param config Pointer to TM1639 configuration structure
 * @param digits Payload of digits to set (0-15)
 * @param length Length of the digits array (should be 8)
 * @param dot_position Decimal point state (true/false)
 * @return int Error code, 0 if successful
 */
tm1639_result_t tm1639_set_digits(output_driver_t *config, const uint8_t* digits, const size_t length, const uint8_t dot_position);

/**
 * @brief Set an entire row in matrix mode
 *
 * @param config Pointer to TM1639 configuration structure
 * @param row Row index (0-7)
 * @param data 8-bit data for the row
 * @return int Error code, 0 if successful
 */
int8_t tm1639_set_matrix_row(output_driver_t *config, uint8_t row, uint8_t data);

/**
 * @brief Deinitialize the TM1639 driver and release resources
 *
 * @param config Pointer to TM1639 configuration structure
 * @return int Error code, 0 if successful
 */
tm1639_result_t tm1639_deinit(output_driver_t *config);

/**
 * @brief Update a specific byte in the preparation buffer.
 *
 * This function updates the value at a specific address in the TM1639 preparation buffer.
 * The preparation buffer is used to stage data before it is sent to the display.
 * All input parameters are validated according to MISRA and SonarQube recommendations.
 *
 * @param[in,out] config Pointer to the TM1639 output driver configuration structure. Must not be NULL.
 * @param[in]     addr   Address to update (valid range: 0x00 to 0x0F).
 * @param[in]     data   Byte value to write to the preparation buffer.
 *
 * @return TM1639_OK on success,
 *         TM1639_ERR_INVALID_PARAM if config is NULL,
 *         TM1639_ERR_ADDRESS_RANGE if addr is out of range.
 */
tm1639_result_t tm1639_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data);

 #endif /* TM1639_H */
