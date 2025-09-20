/**
 * @file tm1637.h
 * @brief TM1637 LED driver helpers built on top of the outputs subsystem.
 * @ingroup outputs
 */

#ifndef TM1637_H
#define TM1637_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "outputs.h"
#include "pico/stdlib.h"
#include "semphr.h"

/**
 * @name TM1637 command opcodes
 * @{
 */
#define TM1637_CMD_DATA_WRITE      0x40U /**< Write data to display register. */
#define TM1637_CMD_FIXED_ADDR      0x44U /**< Fixed address mode. */
#define TM1637_CMD_DISPLAY_OFF     0x80U /**< Display off command. */
#define TM1637_CMD_DISPLAY_ON      0x88U /**< Display on with brightness bits. */
#define TM1637_CMD_ADDR_BASE       0xC0U /**< Base address command. */
/** @} */

/** Total number of bytes exposed by the TM1637 display memory. */
#define TM1637_MAX_DISPLAY_REGISTERS 6U
/** Number of digits supported by the TM1637 controller. */
#define TM1637_DIGIT_COUNT 4U
/** Size of the shadow buffers maintained by the driver. */
#define TM1637_DISPLAY_BUFFER_SIZE 6U
/** Mask applied to toggle the decimal point bit. */
#define TM1637_DECIMAL_POINT_MASK 0x80U
/** Special constant indicating that no decimal point should be rendered. */
#define TM1637_NO_DECIMAL_POINT 0xFFU
/** Mask used to extract the BCD nibble from an encoded digit. */
#define TM1637_BCD_MASK 0x0FU
/** Highest valid BCD value accepted by the driver. */
#define TM1637_BCD_MAX_VALUE 9U

/**
 * @enum tm1637_result_t
 * @brief Result codes for TM1637 driver functions.
 */
typedef enum tm1637_result_t {
        TM1637_OK = 0,            /**< Operation completed successfully. */
        TM1637_ERR_GPIO_INIT = 1, /**< GPIO initialisation error. */
        TM1637_ERR_WRITE = 2,     /**< Error writing data. */
        TM1637_ERR_INVALID_PARAM = 3, /**< Invalid parameter provided to function. */
        TM1637_ERR_ADDRESS_RANGE = 4, /**< Address out of allowed range. */
        TM1637_ERR_INVALID_CHAR = 5,  /**< Invalid character provided. */
        TM1637_ERR_ACK = 6            /**< Acknowledgment error from device. */
} tm1637_result_t;

/**
 * @brief Key scan descriptor returned by @ref tm1637_get_key_states().
 */
typedef struct tm1637_key_t {
        uint8_t ks; /**< Key scan line (1-4). */
        uint8_t k;  /**< Key input line (0-1). */
} tm1637_key_t;

/**
 * @brief Allocate and configure a TM1637 driver instance.
 *
 * The helper initialises the driver bookkeeping structure, clears the attached
 * display, programs the default brightness level and enables the panel. The
 * returned handle is owned by the caller and must later be released with
 * @ref vPortFree() after calling @ref tm1637_deinit().
 *
 * @param[in] chip_id          Logical multiplexer slot that selects the device.
 * @param[in] select_interface Callback used to acquire and release the shared
 *                             bus before transactions.
 * @param[in] spi              SPI instance associated with the shared fabric.
 * @param[in] dio_pin          GPIO index used for the TM1637 DIO signal.
 * @param[in] clk_pin          GPIO index used for the TM1637 CLK signal.
 *
 * @return Pointer to an initialised @ref output_driver_t on success or NULL
 *         when allocation or hardware setup fails.
 */
output_driver_t *tm1637_init(uint8_t chip_id,
                             output_result_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin);

/**
 * @brief Update the TM1637 display with a new set of digits.
 *
 * The digit buffer must contain @ref TM1637_DIGIT_COUNT BCD-encoded entries.
 * The decimal point flag selects a single digit position to illuminate or can
 * be set to @ref TM1637_NO_DECIMAL_POINT to leave all separators disabled.
 *
 * @param[in,out] config       Driver handle obtained from @ref tm1637_init().
 * @param[in]     digits       Pointer to the BCD digit array to render.
 * @param[in]     length       Number of bytes available in @p digits.
 * @param[in]     dot_position Index of the digit that should display the
 *                             decimal point.
 *
 * @retval OUTPUT_OK              Display content was updated successfully.
 * @retval OUTPUT_ERR_INVALID_PARAM One of the parameters is outside the
 *                                  supported range.
 * @retval OUTPUT_ERR_DISPLAY_OUT  Communication with the controller failed.
 */
output_result_t tm1637_set_digits(output_driver_t *config,
                                  const uint8_t *digits,
                                  const size_t length,
                                  const uint8_t dot_position);

/**
 * @brief Update a raw LED register on the TM1637 controller.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1637_init().
 * @param[in]     leds   Register index to update (0 to (@ref TM1637_DISPLAY_BUFFER_SIZE - 1)).
 * @param[in]     ledstate Segment pattern to store at @p leds.
 *
 * @retval OUTPUT_OK              Register contents were committed to hardware.
 * @retval OUTPUT_ERR_INVALID_PARAM The register index is outside the valid range.
 * @retval OUTPUT_ERR_DISPLAY_OUT  Communication with the controller failed.
 */
output_result_t tm1637_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate);

/**
 * @brief Clear the TM1637 display and internal buffers.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1637_init().
 *
 * @retval TM1637_OK             The display memory was cleared successfully.
 * @retval TM1637_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1637_ERR_WRITE      Hardware communication failed.
 */
tm1637_result_t tm1637_clear(output_driver_t *config);

/**
 * @brief Enable the TM1637 display at the configured brightness level.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1637_init().
 *
 * @retval TM1637_OK             The controller acknowledged the command.
 * @retval TM1637_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1637_ERR_WRITE      Hardware communication failed.
 */
tm1637_result_t tm1637_display_on(output_driver_t *config);

/**
 * @brief Disable the TM1637 display without altering cached data.
 *
 * @param[in,out] config Driver handle obtained from @ref tm1637_init().
 *
 * @retval TM1637_OK             The controller acknowledged the command.
 * @retval TM1637_ERR_INVALID_PARAM @p config is NULL.
 * @retval TM1637_ERR_WRITE      Hardware communication failed.
 */
tm1637_result_t tm1637_display_off(output_driver_t *config);

/**
 * @brief Shutdown the TM1637 driver and release its GPIOs.
 *
 * The routine sends a display-off command and returns the dedicated GPIO lines
 * to a safe state. The caller remains responsible for releasing the memory
 * backing @p config with @ref vPortFree().
 *
 * @param[in,out] config Driver handle obtained from @ref tm1637_init().
 *
 * @retval TM1637_OK             Shutdown completed successfully.
 * @retval TM1637_ERR_INVALID_PARAM @p config is NULL.
 */
tm1637_result_t tm1637_deinit(output_driver_t *config);

#endif /* TM1637_H */
