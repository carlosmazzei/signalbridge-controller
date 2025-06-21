#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

/** @def SPI_FREQUENCY
 * @brief SPI frequency for communication with devices.
 */
#define SPI_FREQUENCY (500U * 1000U)

/** @def PWM_PIN
 * @brief Pin used for PWM output.
 * This pin is used to control the brightness of the LEDs.
 */
#define PWM_PIN 28

/** @def MUX_A_PIN, MUX_B_PIN, MUX_C_PIN, MUX_ENABLE
 * @brief Multiplexer control pins.
 * These pins are used to select the SPI interface for communication with the devices.
 * MUX_A_PIN, MUX_B_PIN, and MUX_C_PIN are used to select the chip,
 */
#define MUX_A_PIN 11
#define MUX_B_PIN 12
#define MUX_C_PIN 14
#define MUX_ENABLE 32

/** @def MAX_SPI_INTERFACES
 * @brief Maximum number of SPI interfaces supported.
 * This defines the maximum number of SPI interfaces that can be used in the system.
 */
#define MAX_SPI_INTERFACES 8

/** @def NUM_GPIO
 * @brief Number of GPIO pins available.
 * This defines the total number of GPIO pins available on the Raspberry Pi Pico.
 */
#define NUM_GPIO 30

/** @defgroup device_types Device Types
 *  @brief Macros representing the types of devices connected to the SPI interface.
 *  @{
 */

/** @def DEVICE_NONE
 * @brief No device connected to the SPI interface.
 */
#define DEVICE_NONE           0

/** @def DEVICE_GENERIC_LED
 * @brief Generic LED device connected to the SPI interface.
 */
#define DEVICE_GENERIC_LED    1

/** @def DEVICE_GENERIC_DIGIT
 * @brief Generic digit display device connected to the SPI interface.
 */
#define DEVICE_GENERIC_DIGIT  2

/** @def DEVICE_TM1639_LED
 * @brief TM1639-based LED device connected to the SPI interface.
 */
#define DEVICE_TM1639_LED     3

/** @def DEVICE_TM1639_DIGIT
 * @brief TM1639-based digit display device connected to the SPI interface.
 */
#define DEVICE_TM1639_DIGIT   4

/** @} */ // end of device_types

/** @def DEVICE_CONFIG
 * @brief Device configuration map for SPI interfaces.
 *
 * This macro defines the device type for each SPI interface.
 * Each entry corresponds to a controller ID (0-7), and the value specifies the device type
 * (e.g., DEVICE_TM1639_DIGIT or DEVICE_NONE).
 *
 * Example usage:
 * @code
 * const uint8_t config[] = DEVICE_CONFIG;
 * @endcode
 *
 * - Device 0-2: TM1639 digit controllers
 * - Device   3: TM1639 led controller
 * - Device 4-7: No device connected
 */
#define DEVICE_CONFIG { \
		DEVICE_TM1639_DIGIT, /* Device 0 */ \
		DEVICE_TM1639_DIGIT, /* Device 1 */ \
		DEVICE_TM1639_DIGIT, /* Device 2 */ \
		DEVICE_TM1639_LED, /* Device 3 */ \
		DEVICE_NONE, /* Device 4 */ \
		DEVICE_NONE, /* Device 5 */ \
		DEVICE_NONE, /* Device 6 */ \
		DEVICE_NONE /* Device 7 */ \
}

/** @def OUTPUT_OK
 * @brief Operation successful.
 * This constant indicates that the output operation was successful and no errors occurred.
 */
#define OUTPUT_OK                  0     // Operation successful

/** @def OUTPUT_ERR_INIT
 * @brief Initialization error.
 * This error occurs when the output system fails to initialize properly.
 */
#define OUTPUT_ERR_INIT            1     // Initialization error

/** @def OUTPUT_ERR_DISPLAY_OUT
 * @brief Display out error.
 * This error occurs when there is an issue with sending data to the display.
 */
#define OUTPUT_ERR_DISPLAY_OUT     2     // Display out error

/** @def OUTPUT_ERR_INVALID_PARAM
 * @brief Invalid parameter error.
 * This error occurs when an invalid parameter is passed to a function.
 */
#define OUTPUT_ERR_INVALID_PARAM   3     // Invalid parameter

/** @def OUTPUT_ERR_SEMAPHORE
 * @brief Semaphore error.
 * This error occurs when there is an issue with acquiring or releasing a semaphore.
 */
#define OUTPUT_ERR_SEMAPHORE       4     // Semaphore error

/** --- Statistics Structures --- */

/** @enum out_statistics_counter_enum_t
 * @brief Enumerates different error types in the output system.
 */
typedef enum {
	OUT_CONTROLLER_ID_ERROR,
	OUT_DRIVER_INIT_ERROR,
	OUT_NUM_STATISTICS_COUNTERS /**< Number of statistics counters */
} out_statistics_counter_enum_t;

/** @struct out_statistics_counters_t
 * @brief Holds counters for different error types of the output.
 */
typedef struct out_statistics_counters_t {
	uint32_t counters[OUT_NUM_STATISTICS_COUNTERS]; /**< Array of statistics counters */
	bool error_state;                  /**< Flag indicating critical error state */
} out_statistics_counters_t;

extern out_statistics_counters_t out_statistics_counters;

/* --- Driver Structures --- */

/** @struct output_driver_t
 * @brief Holds the configuration for an output device.
 */
struct output_driver_t;
typedef struct output_driver_t output_driver_t;

struct output_driver_t {
	/* Chip ID (0-7) */
	uint8_t chip_id;

	/* Function pointer for chip selection (true = select/stb low, false = deselect/stb high) */
	uint8_t (*select_interface)(uint8_t chip_id, bool select);
	void (*set_digits)(output_driver_t *config, const uint8_t* digits, const size_t length, const uint8_t dot_position);
	void (*set_leds)(output_driver_t *config, const uint8_t leds, const uint8_t len);

	/* SPI instance */
	spi_inst_t *spi;
	uint8_t dio_pin;
	uint8_t clk_pin;

	/* SPI buffer */
	uint8_t active_buffer[16]; // Active display buffer
	uint8_t prep_buffer[16]; // Preparation buffer for double buffering
	bool buffer_modified;    // Flag indicating if the prep buffer has changed
	uint8_t brightness;      // Brightness level (0-7)
	bool display_on;         // Display on/off
};

/** @struct output_drivers_t
 * @brief Manages a set of output drivers.
 */
typedef struct output_drivers_t {
	output_driver_t *driver_handles[MAX_SPI_INTERFACES];
} output_drivers_t;

extern output_drivers_t output_drivers;

/**
 * Function Prototypes
 */

/** @brief Output the state of the LEDs to the SPI bus.
 *
 * @param payload The data to send.
 * @param length The length of the data to send.
 *
 * @return Error code.
 */
uint8_t led_out(const uint8_t *payload, uint8_t length);

/** @brief Initialize the outputs.
 *
 * Initialize all the outputs needed for the application: SPI, LEDs, PWM.
 *
 * @return Error code.
 * @note This function initializes the SPI bus, sets up the multiplexer for chip selection,
 *
 */
uint8_t output_init(void);

/** @brief Send data to display controllers
 *
 * @param payload The data to send.
 * @param length The length of the data to send.
 *
 * @return Bytes written
 */
uint8_t display_out(const uint8_t *payload, uint8_t length);

/** @brief Set PWM duty cycle
 *
 * @param duty The duty cycle to set.
 */
void set_pwm_duty(uint8_t duty);

#endif