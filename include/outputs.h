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

/** @defgroup mux_pins Multiplexer Pins
 *  @brief Macros for multiplexer control pins.
 *  @{
 */
/** @def SPI_MUX_A_PIN
 * @brief Multiplexer control pin A.
 */
#define SPI_MUX_A_PIN 10
/** @def SPI_MUX_B_PIN
 * @brief Multiplexer control pin B.
 */
#define SPI_MUX_B_PIN 14
/** @def SPI_MUX_C_PIN
 * @brief Multiplexer control pin C.
 */
#define SPI_MUX_C_PIN 15
/** @def SPI_MUX_CS
 * @brief Multiplexer enable pin.
 * This pin is used to enable the multiplexer for chip selection.
 */
#define SPI_MUX_CS 32
/** @} */ // end of mux_pins

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

/** @enum output_result_t
 * @brief Output Result Codes
 * This enum defines the possible result codes returned by output functions.
 * Each code indicates the success or type of error encountered during operations.
 * @note These codes are used to indicate the status of output operations such as initialization and display updates.
 * @ingroup outputs
 */
typedef enum output_result_t {
	OUTPUT_OK = 0,
	OUTPUT_ERR_INIT = 1,
	OUTPUT_ERR_DISPLAY_OUT = 2,
	OUTPUT_ERR_INVALID_PARAM = 3,
	OUTPUT_ERR_SEMAPHORE = 4,
} output_result_t;

/** --- Statistics Structures --- */

/** @enum out_statistics_counter_enum_t
 * @brief Enumerates different error types in the output system.
 */
typedef enum out_statistics_counter_enum_t {
	OUT_CONTROLLER_ID_ERROR,
	OUT_INIT_ERROR,
	OUT_DRIVER_INIT_ERROR,
	OUT_INVALID_PARAM_ERROR,
	OUT_NUM_STATISTICS_COUNTERS /**< Number of statistics counters */
} out_statistics_counter_enum_t;

/** @struct out_statistics_counters_t
 * @brief Holds counters for different error types of the output.
 */
typedef struct out_statistics_counters_t {
	uint32_t counters[OUT_NUM_STATISTICS_COUNTERS]; /**< Array of statistics counters */
	bool error_state;                  /**< Flag indicating critical error state */
} out_statistics_counters_t;


// --- Driver Structures ---

/** @struct output_driver_t
 * @brief Holds the configuration for an output device.
 */
struct output_driver_t;
typedef struct output_driver_t output_driver_t;

struct output_driver_t {
	// Chip ID (0-7)
	uint8_t chip_id;

	// Function pointer for chip selection (true = select/stb low, false = deselect/stb high)
	output_result_t (*select_interface)(uint8_t chip_id, bool select);
	output_result_t (*set_digits)(output_driver_t *config, const uint8_t* digits, const size_t length, const uint8_t dot_position);
	output_result_t (*set_leds)(output_driver_t *config, const uint8_t leds, const uint8_t ledstate);

	// SPI instance
	spi_inst_t *spi;
	uint8_t dio_pin;
	uint8_t clk_pin;

	// SPI buffer
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


/**
 * Function Prototypes
 */

/**
 * @brief Sends LED state information to the appropriate output driver.
 *
 * The expected payload format is:
 * - Byte 0: Controller ID (1-based)
 * - Byte 1: LED index
 * - Byte 2: LED state (0-255)
 *
 * @param[in] payload Pointer to the payload buffer.
 * @param[in] length  Length of the payload buffer (should be at least 3).
 * @return OUTPUT_OK on success, or an error code on failure.
 */
output_result_t led_out(const uint8_t *payload, uint8_t length);

/** @brief Initialize the outputs.
 *
 * Initialize all the outputs needed for the application: SPI, LEDs, PWM.
 *
 * @return Error code.
 * @note This function initializes the SPI bus, sets up the multiplexer for chip selection,
 *
 */
output_result_t output_init(void);

/**
 * @brief Sends a BCD-encoded digit payload to the appropriate output driver.
 *
 * The expected payload format is:
 * - Byte 0: Controller ID (1-based)
 * - Bytes 1-4: Packed BCD digits (2 digits per byte, lower and upper nibble)
 * - Byte 5: Dot position
 *
 * @param[in] payload Pointer to the payload buffer.
 * @param[in] length  Length of the payload buffer (should be at least 6).
 * @return OUTPUT_OK on success, or an error code on failure.
 */
output_result_t display_out(const uint8_t *payload, uint8_t length);

/** @brief Set PWM duty cycle
 *
 * @param duty The duty cycle to set.
 */
void set_pwm_duty(uint8_t duty);

#endif
