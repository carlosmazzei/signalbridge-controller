#ifndef _OUTPUTS_H_
#define _OUTPUTS_H_

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

/**
 * SPI frequency in Hz
 */
#define SPI_FREQUENCY 500 * 1000

/**
 * SPI buffer length
 */
#define SPI_BUF_LEN 10

/**
 * PWM pin
 */
#define PWM_PIN 28

/**
 * MUX PINS
 */
#define MUX_A_PIN 11
#define MUX_B_PIN 12
#define MUX_C_PIN 14
#define MUX_ENABLE 32

/* Constants */
#define MAX_SPI_INTERFACES 8
#define NUM_GPIO 30

/* Device type definitions */
#define DEVICE_NONE           0
#define DEVICE_GENERIC_LED    1
#define DEVICE_GENERIC_DIGIT  2
#define DEVICE_TM1639_LED     3
#define DEVICE_TM1639_DIGIT   4

/* Device configuration (one byte per interface). Use position as controller ID. */
#define DEVICE_CONFIG { \
		DEVICE_TM1639_DIGIT, /* Device 0 */ \
		DEVICE_TM1639_DIGIT, /* Device 1 */ \
		DEVICE_TM1639_DIGIT, /* Device 2 */ \
		DEVICE_TM1639_DIGIT, /* Device 3 */ \
		DEVICE_NONE, /* Device 4 */ \
		DEVICE_NONE, /* Device 5 */ \
		DEVICE_NONE, /* Device 6 */ \
		DEVICE_NONE /* Device 7 */ \
}
/** --- Statistics Structures --- */

/**
 * @enum out_statistics_counter_enum_t
 * @brief Enumerates different error types in the output system.
 */
typedef enum out_statistics_counter_enum_t
{
	OUT_CONTROLLER_ID_ERROR,
	OUT_DRIVER_INIT_ERROR,
	OUT_NUM_STATISTICS_COUNTERS /**< Number of statistics counters */
} out_statistics_counter_enum_t;

/**
 * @struct out_statistics_counters_t
 * @brief Holds counters for different error types of the output.
 */
typedef struct out_statistics_counters_t
{
	uint32_t counters[OUT_NUM_STATISTICS_COUNTERS]; /**< Array of statistics counters */
	bool error_state;                  /**< Flag indicating critical error state */
} out_statistics_counters_t;

/* --- Driver Structures --- */

/**
 * @struct output_driver_t
 * @brief Holds the configuration for an output device.
 */
typedef struct output_driver_t {
	/* Chip ID (0-7) */
	uint8_t chip_id;

	/* Function pointer for chip selection (true = select/stb low, false = deselect/stb high) */
	void (*select_interface)(uint8_t chip_id, bool select);
	void (*set_digits)(uint8_t *digits, uint8_t len);
	void (*set_leds)(uint8_t *leds, uint8_t len);

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
} output_driver_t;

/**
 * @struct output_drivers_t
 * @brief Manages a set of output drivers.
 */
typedef struct output_drivers_t {
	output_driver_t *driver_handles[MAX_SPI_INTERFACES];
} output_drivers_t;

/**
 * Function Prototypes
 */

/** @brief Output the state of the LEDs to the SPI bus.
 *
 * @param payload The data to send.
 * @param length The length of the data to send.
 *
 * @return True if successful.
 */
int led_out(const uint8_t *payload, uint8_t length);

/** @brief Initialize the outputs.
 *
 * Initialize all the outputs needed for the application: SPI, LEDs, PWM.
 *
 * @return True if successful.
 *
 * @todo Implement the I2C initialization.
 *
 */
bool output_init(void);

/** @brief Send data to display controllers
 *
 * @param payload The data to send.
 * @param length The length of the data to send.
 *
 * @return Bytes written
 */
int display_out(const uint8_t *payload, uint8_t length);

/** @brief Set PWM duty cycle
 *
 * @param duty The duty cycle to set.
 */
void set_pwm_duty(uint8_t duty);

/**
 * @brief Select the interace chip through multiplexer
 *
 * @param chip_select Chip select number (0-7)
 * @param select True to select (STB low), false to deselect (STB high)
 */
static void select_interface(uint8_t chip_select, bool select);

/**
 * @brief Initialize the multiplexer for chip select control
 *
 * @return int Error code, 0 if successful
 */
static int8_t init_mux(void);

#endif