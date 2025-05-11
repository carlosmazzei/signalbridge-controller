#ifndef _OUTPUTS_H_
#define _OUTPUTS_H_

#include <stdint.h>
#include <stdbool.h>

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

/**
 * @enum out_statistics_counter_enum_t
 * @brief Enumerates different error types in the output system.
 */
typedef enum out_statistics_counter_enum_t
{
	OUT_CONTROLLER_ID_ERROR,
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

/**
 * Function Prototypes
 */

/** @brief Send data to LED controllers
 *
 * @param index The index of the LED controller.
 * @param states The states to send.
 * @param len The length of the states.
 *
 * @return true if successful, false otherwise
 */
bool led_out(uint8_t index, const uint8_t *states, uint8_t len);

/** @brief Select the LED controller */
void led_select(void);

/** @brief Initialize the output system */
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

#endif