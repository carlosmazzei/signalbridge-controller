/**
 * @file error_management.h
 * @brief Error management and LED indication for system errors
 * @author Carlos Mazzei
 *
 * @date 2020-2025
 * @copyright
 *   (c) 2020-2025 Carlos Mazzei - All rights reserved.
 *
 */

#ifndef ERROR_MANAGEMENT_H
#define ERROR_MANAGEMENT_H

#include <stdint.h>
#include <stdbool.h>

// Watchdog timer delay
#define WATCHDOG_GRACE_PERIOD_MS 5000U

// Error LED configuration
#define ERROR_LED_PIN PICO_DEFAULT_LED_PIN

// Timing constants (milliseconds)
#define BLINK_ON_MS      150     // LED on time
#define BLINK_OFF_MS     150     // Time between blinks in pattern
#define PATTERN_PAUSE_MS 2000    // Pause between pattern repeats

// Duration to show error before a tentative restart (must be < watchdog grace)
#define ERROR_DISPLAY_BEFORE_TENTATIVE_RESTART_MS 12000

// Watchdog scratch register usage
#define WATCHDOG_ERROR_COUNT_REG       0

/**
 * @enum error_type_t
 * @brief Enumerates different error types in the system.
 */
typedef enum {
	ERROR_NONE = 0,
	ERROR_WATCHDOG_TIMEOUT = 1,
	ERROR_FREERTOS_STACK = 2,
	ERROR_PICO_SDK_PANIC = 3,
	ERROR_SCHEDULER_FAILED = 4,
	ERROR_RESOURCE_ALLOCATION = 5,
	ERROR_USB_INIT = 6,
} error_type_t;

/**
 * @enum statistics_counter_enum_t
 * @brief Enumerates different error types in the system.
 */
typedef enum statistics_counter_enum_t {
	// Main error enums
	QUEUE_SEND_ERROR,
	QUEUE_RECEIVE_ERROR,
	CDC_QUEUE_SEND_ERROR,
	DISPLAY_OUT_ERROR,
	LED_OUT_ERROR,
	WATCHDOG_ERROR,
	MSG_MALFORMED_ERROR,
	COBS_DECODE_ERROR,
	RECEIVE_BUFFER_OVERFLOW_ERROR,
	CHECKSUM_ERROR,
	BUFFER_OVERFLOW_ERROR,
	UNKNOWN_CMD_ERROR,
	BYTES_SENT,
	BYTES_RECEIVED,

	// Recovery-related enums
	RECOVERY_ATTEMPTS_EXCEEDED,
	RECOVERY_HEAP_ERROR,

	// Output error enums
	OUTPUT_CONTROLLER_ID_ERROR,
	OUTPUT_INIT_ERROR,
	OUTPUT_DRIVER_INIT_ERROR,
	OUTPUT_INVALID_PARAM_ERROR,

	// Input error enums
	INPUT_QUEUE_INIT_ERROR,
	INPUT_INIT_ERROR,

	NUM_STATISTICS_COUNTERS /**< Number of statistics counters */
} statistics_counter_enum_t;

/**
 * @struct statistics_counters_t
 * @brief Holds counters for different error types.
 */
typedef struct statistics_counters_t {
	uint32_t counters[NUM_STATISTICS_COUNTERS]; /**< Array of statistics counters */
	bool error_state;                  /**< Flag indicating critical error state */
	error_type_t current_error_type;
} statistics_counters_t;

/**
 * @brief Increment a statistics counter.
 *
 * @param[in] index Counter index to increment.
 */
void statistics_increment_counter(statistics_counter_enum_t index);

/**
 * @brief Add a value to a statistics counter.
 *
 * @param[in] index Counter index to update.
 * @param[in] value Value to add.
 */
void statistics_add_to_counter(statistics_counter_enum_t index, uint32_t value);

/**
 * @brief Set a statistics counter to a specific value.
 *
 * @param[in] index Counter index to set.
 * @param[in] value Value to assign.
 */
void statistics_set_counter(statistics_counter_enum_t index, uint32_t value);

/**
 * @brief Retrieve the value of a statistics counter.
 *
 * @param[in] index Counter index to read.
 * @return Current counter value.
 */
uint32_t statistics_get_counter(statistics_counter_enum_t index);

/**
 * @brief Reset all statistics counters.
 */
void statistics_reset_all_counters(void);

/**
 * @brief Check if the system is in an error state.
 *
 * @return true if in error state, false otherwise.
 */
bool statistics_is_error_state(void);

/**
 * @brief Get the current error type.
 *
 * @return Current error type.
 */
error_type_t statistics_get_error_type(void);

/**
 * @brief Clear the error state and set error type to ERROR_NONE.
 */
void statistics_clear_error(void);

/**
 * @brief Record a recoverable error for diagnostics.
 *
 * @param[in] type Recoverable error type being raised.
 */
void error_management_record_recoverable(error_type_t type);

/**
 * @brief Record a fatal error without entering recovery mode.
 *
 * @param[in] type Fatal error type to persist for diagnostics.
 */
void error_management_record_fatal(error_type_t type);

/**
 * @brief Immediately halt execution and display a fatal error pattern.
 *
 * This function does not return. It disables interrupts and enters
 * an infinite loop displaying the error pattern for the specified error type.
 *
 */
void __attribute__((noreturn)) fatal_halt(error_type_t type);

/**
 * @brief Check if an error type supports automatic recovery.
 */
bool error_management_is_recoverable(error_type_t type);

/**
 * @brief Show a blinking error pattern on the error LED.
 *
 * @param[in] error_type The type of error to display.
 */
void show_error_pattern_blocking(error_type_t error_type);

/**
 * @brief Show error pattern for a bounded duration while servicing watchdog.
 *
 * Repeats the current error LED pattern and calls `update_watchdog_safe()`
 * until `duration_ms` elapses. Use a value comfortably below the watchdog
 * error grace in `update_watchdog_safe()` (currently 15000 ms).
 *
 * @param[in] duration_ms Duration to display the pattern, in milliseconds.
 */
void show_error_for_duration_ms(uint32_t duration_ms);

/**
 * @brief Set the up watchdog with error detection object
 *
 * @param[in] timeout_ms Timeout value in milliseconds
 */
void setup_watchdog_with_error_detection(uint32_t timeout_ms);

#endif // ERROR_MANAGEMENT_H
