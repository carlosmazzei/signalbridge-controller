/**
 * @file tm1639.h
 * @brief TM1639 LED driver helpers that leverage the shared outputs subsystem.
 * @ingroup outputs
 */

#ifndef TM1639_H
#define TM1639_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include <hardware/spi.h>
#include "app_outputs.h"
#include <pico/stdlib.h>
#include "semphr.h"

/**
 * @name TM1639 command opcodes
 * @{
 */
#define TM1639_CMD_DATA_WRITE      0x02U /**< Auto-increment data write (bit-reversed for MSB-first SPI). */
#define TM1639_CMD_DATA_READ_KEYS  0x42U /**< Key scanning data read (bit-reversed for MSB-first SPI). */
#define TM1639_CMD_FIXED_ADDR      0x22U /**< Fixed address write (bit-reversed for MSB-first SPI). */
#define TM1639_CMD_DISPLAY_OFF     0x01U /**< Display off command (bit-reversed for MSB-first SPI). */
#define TM1639_CMD_DISPLAY_ON      0x11U /**< Display on command base (bit-reversed for MSB-first SPI). */
#define TM1639_CMD_ADDR_BASE       0x03U /**< Base address command (bit-reversed for MSB-first SPI). */
/** @} */

/** Number of TM1639 display registers. */
#define TM1639_MAX_DISPLAY_REGISTERS 16U
/** Number of seven-segment digits exposed by the chip. */
#define TM1639_DIGIT_COUNT 8U
/** Size of the driver staging buffers. */
#define TM1639_DISPLAY_BUFFER_SIZE 16U
/** Mask to toggle the decimal point bit. */
#define TM1639_DECIMAL_POINT_MASK 0x80U
/** Constant to indicate that the decimal point is inactive. */
#define TM1639_NO_DECIMAL_POINT 0xFFU
/** Mask applied to extract the BCD nibble from a digit. */
#define TM1639_BCD_MASK 0x0FU
/** Maximum valid BCD digit accepted by the driver. */
#define TM1639_BCD_MAX_VALUE 9U

/**
 * @enum tm1639_result_t
 * @brief Result codes for TM1639 driver functions.
 */
typedef enum tm1639_result_t {
	TM1639_OK = 0,             /**< Operation completed successfully. */
	TM1639_ERR_SPI_INIT = 1,   /**< SPI initialisation error. */
	TM1639_ERR_GPIO_INIT = 2,  /**< GPIO initialisation error. */
	TM1639_ERR_SPI_WRITE = 3,  /**< SPI write failed. */
	TM1639_ERR_INVALID_PARAM = 4, /**< Invalid parameter provided to function. */
	TM1639_ERR_ADDRESS_RANGE = 5, /**< Address out of allowed range. */
	TM1639_ERR_MUTEX_TIMEOUT = 6, /**< Timeout while acquiring mutex. */
	TM1639_ERR_INVALID_CHAR = 7   /**< Invalid character provided. */
} tm1639_result_t;

/**
 * @brief Key scan descriptor returned by @ref tm1639_get_key_states().
 */
typedef struct tm1639_key_t {
	uint8_t ks; /**< Key scan line (1-4). */
	uint8_t k;  /**< Key input line (0-1). */
} tm1639_key_t;

/**
 * @brief Allocate and configure a TM1639 driver instance.
 *
 * The helper initialises the driver bookkeeping structure, clears the display,
 * programs the default brightness level and enables the panel. The returned
 *
 * @param[in] chip_id          Logical multiplexer slot that selects the device.
 * @param[in] select_interface Callback used to acquire and release the shared
 *                             bus before transactions.
 * @param[in] spi              SPI instance that drives the controller.
 * @param[in] dio_pin          GPIO index wired to the TM1639 data input (MOSI).
 * @param[in] clk_pin          GPIO index wired to the TM1639 clock input.
 *
 * @return Pointer to an initialised @ref output_driver_t on success or NULL
 *         when allocation or hardware setup fails.
 */
output_driver_t *tm1639_init(uint8_t chip_id,
                             output_result_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin);

/**
 * @brief Enable the TM1639 display at the configured brightness level.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 *
 * @retval TM1639_OK              The controller acknowledged the command.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_SPI_WRITE   The command write failed.
 */
tm1639_result_t tm1639_display_on(output_driver_t *config);

/**
 * @brief Disable the TM1639 display without altering cached data.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 *
 * @retval TM1639_OK              The controller acknowledged the command.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_SPI_WRITE   The command write failed.
 */
tm1639_result_t tm1639_display_off(output_driver_t *config);

/**
 * @brief Clear the TM1639 display and internal buffers.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 *
 * @retval TM1639_OK              The display memory was cleared successfully.
 * @retval TM1639_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1639_ERR_SPI_WRITE   Communication with the controller failed.
 */
tm1639_result_t tm1639_clear(output_driver_t *config);

/**
 * @brief Render a BCD digit buffer on the TM1639 display.
 *
 * The digit buffer must contain @ref TM1639_DIGIT_COUNT entries and may request
 * a single decimal point through @p dot_position or
 * @ref TM1639_NO_DECIMAL_POINT to leave the separators disabled.
 *
 * @param[in,out] config       Driver handle obtained from @ref tm1639_init().
 * @param[in]     digits       Pointer to the BCD digit array to render.
 * @param[in]     length       Number of bytes available in @p digits.
 * @param[in]     dot_position Index of the digit that should display the decimal
 *                             point.
 *
 * @retval OUTPUT_OK              Display content was updated successfully.
 * @retval OUTPUT_ERR_INVALID_PARAM One of the parameters is outside the
 *                                  supported range.
 * @retval OUTPUT_ERR_DISPLAY_OUT  Communication with the controller failed.
 */
output_result_t tm1639_set_digits(output_driver_t *config,
                                  const uint8_t *digits,
                                  const size_t length,
                                  const uint8_t dot_position);

/**
 * @brief Update a raw LED register on the TM1639 controller.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1639_init().
 * @param[in]     leds   Register index to update (0-15).
 * @param[in]     ledstate Segment pattern to store at @p leds.
 *
 * @retval OUTPUT_OK              Register contents were committed to hardware.
 * @retval OUTPUT_ERR_INVALID_PARAM The register index is outside the valid range.
 * @retval OUTPUT_ERR_DISPLAY_OUT  Communication with the controller failed.
 */
output_result_t tm1639_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate);

#endif // TM1639_H
