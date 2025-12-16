#ifndef OUTPUTS_H
#define OUTPUTS_H

/**
 * @file app_outputs.h
 * @brief Interfaces for controlling LED, display and PWM output devices.
 *
 * The outputs subsystem coordinates SPI-attached LED controllers (TM1639) and
 * bit-banged TM1637 displays while providing helper routines to update PWM
 * brightness channels and individual annunciators.  Payloads received from the
 * host are validated, decoded and passed to the concrete driver instances
 * initialised by @ref output_init().
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hardware/spi.h>

/**
 * @defgroup outputs Outputs subsystem
 * @brief Configuration constants and APIs for driving panel outputs.
 * @{
 */

/**
 * @name SPI fabric configuration
 * @{
 */
/** Nominal SPI bus frequency used for TM1639 devices (500 kHz). */
#define SPI_FREQUENCY (500U * 1000U)
/** Total number of logical SPI interfaces exposed through the multiplexer. */
#define MAX_SPI_INTERFACES 8U
/** Number of GPIOs exposed by the RP2040 package. */
#define NUM_GPIO 30U
/** @} */

/**
 * @name Multiplexer GPIO assignments
 * @{
 */
/** Multiplexer select bit 0 for SPI device routing. */
#define SPI_MUX_A_PIN 10U
/** Multiplexer select bit 1 for SPI device routing. */
#define SPI_MUX_B_PIN 14U
/** Multiplexer select bit 2 for SPI device routing. */
#define SPI_MUX_C_PIN 15U
/** Multiplexer enable pin (active high). */
#define SPI_MUX_CS 27U
/** @} */

/**
 * @name PWM configuration
 * @{
 */
/** GPIO used for the global PWM brightness channel. */
#define PWM_PIN 28U
/** @} */

/**
 * @name Logical device identifiers
 * @{
 */
/** No device is fitted for the given slot. */
#define DEVICE_NONE 0U
/** Generic LED sink without a dedicated driver. */
#define DEVICE_GENERIC_LED 1U
/** Generic seven-segment digit controller. */
#define DEVICE_GENERIC_DIGIT 2U
/** TM1639 LED matrix using the SPI fabric. */
#define DEVICE_TM1639_LED 3U
/** TM1639 seven-segment digit controller. */
#define DEVICE_TM1639_DIGIT 4U
/** TM1637 LED matrix driven through bit-banging. */
#define DEVICE_TM1637_LED 5U
/** TM1637 seven-segment digit controller. */
#define DEVICE_TM1637_DIGIT 6U
/** @} */

/**
 * @brief Compile-time device assignment for each controller slot.
 *
 * The table is indexed by the 1-based controller identifier received from the
 * host (minus one) and selects the driver implementation that should handle a
 * given payload.
 */
#define DEVICE_CONFIG { \
		DEVICE_TM1639_DIGIT, /* Device 0 */ \
		DEVICE_TM1637_DIGIT, /* Device 1 */ \
		DEVICE_TM1639_DIGIT, /* Device 2 */ \
		DEVICE_TM1639_LED, /* Device 3 */ \
		DEVICE_NONE, /* Device 4 */ \
		DEVICE_NONE, /* Device 5 */ \
		DEVICE_NONE, /* Device 6 */ \
		DEVICE_NONE  /* Device 7 */ \
}

/**
 * @brief Result codes returned by output helpers.
 */
typedef enum output_result_t {
	OUTPUT_OK = 0,             /**< Operation completed successfully. */
	OUTPUT_ERR_INIT = 1,       /**< Hardware initialisation failed. */
	OUTPUT_ERR_DISPLAY_OUT = 2,/**< Display or LED driver rejected the payload. */
	OUTPUT_ERR_INVALID_PARAM = 3, /**< Payload validation failed. */
	OUTPUT_ERR_SEMAPHORE = 4   /**< Failed to acquire the SPI mutex. */
} output_result_t;

/**
 * @brief Forward declaration for the polymorphic output driver structure.
 */
struct output_driver_t;
typedef struct output_driver_t output_driver_t;

/**
 * @brief Abstraction around a concrete display or LED driver instance.
 */
struct output_driver_t {
	uint8_t chip_id; /**< Logical identifier mapped to the multiplexer select lines. */
	output_result_t (*select_interface)(uint8_t chip_id, bool select); /**< Mux control callback. */
	output_result_t (*set_digits)(output_driver_t *config, const uint8_t *digits, size_t length, uint8_t dot_position); /**< Digit update callback. */
	output_result_t (*set_leds)(output_driver_t *config, uint8_t leds, uint8_t ledstate); /**< LED update callback. */
	spi_inst_t *spi; /**< SPI instance used by the device (if applicable). */
	uint8_t dio_pin; /**< GPIO pin used as DIO for TM1637 bit-banging. */
	uint8_t clk_pin; /**< GPIO pin used as CLK for TM1637 bit-banging. */
	uint8_t active_buffer[16]; /**< Snapshot of the last committed frame. */
	uint8_t prep_buffer[16];   /**< Staging buffer used before flushing to hardware. */
	bool buffer_modified;      /**< Indicates that @ref prep_buffer needs flushing. */
	uint8_t brightness;        /**< Driver brightness level (0-7). */
	bool display_on;           /**< Current display state. */
};

/**
 * @brief Container for every driver handle managed by @ref app_outputs.c.
 */
typedef struct output_drivers_t {
	output_driver_t *driver_handles[MAX_SPI_INTERFACES]; /**< Pointer table indexed by chip ID. */
} output_drivers_t;

/** @} */

/**
 * @brief Initialise the SPI bus, PWM slice and driver backends.
 *
 * Configures the SPI fabric, routes the multiplexer control GPIOs, initialises
 * the PWM brightness channel and creates the low-level driver instances defined
 * by @ref DEVICE_CONFIG.
 *
 * @retval OUTPUT_OK         The subsystem is ready to accept payloads.
 * @retval OUTPUT_ERR_INIT   At least one hardware block failed to initialise.
 */
output_result_t output_init(void);

/**
 * @brief Dispatch a display update payload to the matching driver.
 *
 * @param[in] payload Encoded BCD digit stream received from the host.
 * @param[in] length  Number of bytes available in @p payload.
 *
 * @retval OUTPUT_OK            The update was queued successfully.
 * @retval OUTPUT_ERR_INVALID_PARAM The payload failed validation.
 * @retval OUTPUT_ERR_DISPLAY_OUT   Driver rejected the update request.
 * @retval OUTPUT_ERR_SEMAPHORE     SPI bus could not be locked.
 */
output_result_t display_out(const uint8_t *payload, uint8_t length);

/**
 * @brief Dispatch an LED update payload to the matching driver.
 *
 * @param[in] payload Encoded LED controller update received from the host.
 * @param[in] length  Number of bytes available in @p payload.
 *
 * @retval OUTPUT_OK            The update was queued successfully.
 * @retval OUTPUT_ERR_INVALID_PARAM The payload failed validation.
 * @retval OUTPUT_ERR_DISPLAY_OUT   Driver rejected the update request.
 * @retval OUTPUT_ERR_SEMAPHORE     SPI bus could not be locked.
 */
output_result_t led_out(const uint8_t *payload, uint8_t length);

/**
 * @brief Update the PWM duty cycle that controls the LED brightness rail.
 *
 * @param[in] duty 8-bit duty value, squared internally for perceptual linearity.
 */
void set_pwm_duty(uint8_t duty);

#endif // OUTPUTS_H
