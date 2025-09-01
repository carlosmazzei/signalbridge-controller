/**
 * @file tm1637.h
 * @brief TM1637 LED Driver Header for Raspberry Pi Pico
 *
 * This header provides an interface for controlling TM1637 LED driver chips
 * using GPIO bit-banging for the two-wire protocol (CLK and DIO).
 */

#ifndef TM1637_H
#define TM1637_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "outputs.h"

// TM1637 Command Constants
#define TM1637_CMD_DATA_WRITE      0x40  // Write data to display register
#define TM1637_CMD_FIXED_ADDR      0x44  // Fixed address mode
#define TM1637_CMD_DISPLAY_OFF     0x80  // Display off
#define TM1637_CMD_DISPLAY_ON      0x88  // Display on + brightness (0-7)
#define TM1637_CMD_ADDR_BASE       0xC0  // Base address command

#define TM1637_MAX_DISPLAY_REGISTERS    (6U)
#define TM1637_DIGIT_COUNT              (4U)    /* Number of digits supported */
#define TM1637_DISPLAY_BUFFER_SIZE      (6U)    /* Total display buffer size */
#define TM1637_DECIMAL_POINT_MASK       (0x80U) /* Mask for decimal point bit */
#define TM1637_NO_DECIMAL_POINT         (0xFFU) /* Value indicating no decimal point */
#define TM1637_BCD_MASK                 (0x0FU) /* Mask for BCD digit */
#define TM1637_BCD_MAX_VALUE            (9U)     /* Maximum valid BCD digit */

/**
 * @enum tm1637_result_t
 * @brief Result codes for TM1637 driver functions.
 *
 * This enumeration defines the possible return values for TM1637 driver functions,
 * indicating success or the type of error encountered during operations.
 */
typedef enum tm1637_result_t {
	TM1637_OK = 0,            /**< Operation completed successfully. */
	TM1637_ERR_GPIO_INIT = 1, /**< GPIO initialization error. */
	TM1637_ERR_WRITE = 2,     /**< Error writing data. */
	TM1637_ERR_INVALID_PARAM = 3, /**< Invalid parameter provided to function. */
	TM1637_ERR_ADDRESS_RANGE = 4, /**< Address out of allowed range. */
	TM1637_ERR_INVALID_CHAR = 5,  /**< Invalid character provided. */
	TM1637_ERR_ACK = 6        /**< Acknowledgment error from device. */
} tm1637_result_t;

/**
 * @brief Initialize the TM1637 driver.
 *
 * Allocates and initializes a TM1637 output driver structure.
 * Checks all parameters and hardware resources before returning a valid pointer.
 *
 * @param[in] chip_id           Controller ID (0-based).
 * @param[in] select_interface  Function pointer for chip selection (not used for TM1637, can be NULL).
 * @param[in] spi               SPI instance pointer (not used for TM1637, can be NULL).
 * @param[in] dio_pin           DIO GPIO pin number.
 * @param[in] clk_pin           CLK GPIO pin number.
 * @return Pointer to initialized output_driver_t structure, or NULL on error.
 */
output_driver_t* tm1637_init(uint8_t chip_id,
                             output_result_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin);

/**
 * @brief Set BCD digits on the TM1637 display.
 *
 * This function sets BCD-encoded digits on the TM1637 4-digit display. It validates
 * parameters, processes the digits with segment patterns, and updates the display.
 *
 * @param[in,out] config       Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 * @param[in]     digits       Pointer to BCD digit array (4 digits, each 0x00-0x0F).
 * @param[in]     length       Length of the digits array (must be 4 for TM1637).
 * @param[in]     dot_position Decimal point position (0-3) or TM1637_NO_DECIMAL_POINT for none.
 *
 * @return output_result_t OUTPUT_OK on success,
 *                         OUTPUT_ERR_INVALID_PARAM if parameters are invalid,
 *                         OUTPUT_ERR_DISPLAY_OUT for other display errors.
 */
output_result_t tm1637_set_digits(output_driver_t *config, const uint8_t* digits, const size_t length, const uint8_t dot_position);

/**
 * @brief Set LED patterns for the TM1637 display.
 *
 * This function sets LED patterns for the TM1637 display by directly updating
 * the display buffer at the specified address with the LED state data.
 *
 * @param[in,out] config   Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 * @param[in]     leds     LED index or address (0 to TM1637_DISPLAY_BUFFER_SIZE-1).
 * @param[in]     ledstate LED state pattern (0-255, bit pattern for segments).
 *
 * @return output_result_t OUTPUT_OK on success,
 *                         OUTPUT_ERR_INVALID_PARAM if config is NULL,
 *                         OUTPUT_ERR_DISPLAY_OUT for other display errors.
 */
output_result_t tm1637_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate);

/**
 * @brief Clear the TM1637 display (set all segments off).
 *
 * This function clears the TM1637 display by setting all segment memory to zero.
 * It clears both internal buffers and sends clear commands to the hardware.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 *
 * @return tm1637_result_t TM1637_OK on success,
 *                         TM1637_ERR_INVALID_PARAM if config is NULL,
 *                         TM1637_ERR_WRITE if SPI write fails.
 */
tm1637_result_t tm1637_clear(output_driver_t *config);

/**
 * @brief Turn the TM1637 display on.
 *
 * This function turns the TM1637 display on by setting the display_on flag in the driver
 * configuration and sending the display ON command with the current brightness level.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 *
 * @return tm1637_result_t TM1637_OK on success,
 *                         TM1637_ERR_INVALID_PARAM if config is NULL,
 *                         TM1637_ERR_WRITE if command send fails.
 */
tm1637_result_t tm1637_display_on(output_driver_t *config);

/**
 * @brief Turn the TM1637 display off.
 *
 * This function turns the TM1637 display off by setting the display_on flag to false
 * and sending the display OFF command to the device.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 *
 * @return tm1637_result_t TM1637_OK on success,
 *                         TM1637_ERR_INVALID_PARAM if config is NULL,
 *                         TM1637_ERR_WRITE if command send fails.
 */
tm1637_result_t tm1637_display_off(output_driver_t *config);

/**
 * @brief Deinitialize the TM1637 driver and release resources.
 *
 * This function properly shuts down the TM1637 driver by turning off the display
 * and cleaning up any allocated resources.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Can be NULL.
 *
 * @return tm1637_result_t TM1637_OK on success,
 *                         TM1637_ERR_INVALID_PARAM if config is NULL.
 */
tm1637_result_t tm1637_deinit(output_driver_t *config);

#endif // TM1637_H