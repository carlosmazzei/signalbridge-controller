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

// Error LED configuration
#define ERROR_LED_PIN PICO_DEFAULT_LED_PIN

// Timing constants (milliseconds)
#define BLINK_ON_MS      150     // LED on time
#define BLINK_OFF_MS     150     // Time between blinks in pattern
#define PATTERN_PAUSE_MS 2000    // Pause between pattern repeats

// Watchdog scratch register usage
#define WATCHDOG_ERROR_TYPE_REG    0
#define WATCHDOG_ERROR_COUNT_REG   1
#define WATCHDOG_BOOT_MAGIC_REG    2

#define ERROR_MAGIC_VALUE     0xDEADBEEF
#define CLEAN_BOOT_MAGIC      0x600DC0DE

typedef enum {
	ERROR_NONE = 0,
	ERROR_WATCHDOG_TIMEOUT = 1,
	ERROR_FREERTOS_STACK = 2,
	ERROR_PICO_SDK_PANIC = 3,
	ERROR_SCHEDULER_FAILED = 4,
	ERROR_RESOURCE_ALLOCATION = 5
} error_type_t;

/**
 * @enum statistics_counter_enum_t
 * @brief Enumerates different error types in the system.
 */
typedef enum statistics_counter_enum_t {
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
 * @brief Holds various statistics counters for error tracking.
 */
extern volatile statistics_counters_t statistics_counters;

/**
 * @brief Set the error state persistently in the watchdog registers.
 *
 * @param[in] type The type of error to set.
 */
void set_error_state_persistent(error_type_t type);

/**
 * @brief Show a blinking error pattern on the error LED.
 *
 * @param[in] error_type The type of error to display.
 */
void show_error_pattern_blocking(error_type_t error_type);

/**
 * @brief Set the up watchdog with error detection object
 *
 * @param[in] timeout_ms Timeout value in milliseconds
 */
void setup_watchdog_with_error_detection(uint32_t timeout_ms);

/**
 * @brief Update the watchdog timer safely.
 *
 * This function updates the watchdog timer only if the system is not in an error state.
 */
void update_watchdog_safe(void);

#endif // ERROR_MANAGEMENT_H