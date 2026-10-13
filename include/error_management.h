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

/** Hardware watchdog timeout before reset, in milliseconds. */
#define WATCHDOG_GRACE_PERIOD_MS 5000U

/** GPIO pin used to display fatal and recoverable error patterns. */
#define ERROR_LED_PIN PICO_DEFAULT_LED_PIN

/** Error LED on-time for each pulse in a pattern, in milliseconds. */
#define BLINK_ON_MS 150
/** Gap between pulses in an error pattern, in milliseconds. */
#define BLINK_OFF_MS 150
/** Pause between repeated error patterns, in milliseconds. */
#define PATTERN_PAUSE_MS 2000
/** Interval between status LED checks, in milliseconds. */
#define LED_POLL_INTERVAL_MS 100

/** Watchdog scratch register retaining the count of watchdog resets. */
#define WATCHDOG_ERROR_COUNT_REG       0
/** Watchdog scratch register retaining the most recent fatal error type. */
#define WATCHDOG_ERROR_LAST_TYPE_REG   1

/**
 * @typedef error_type_t
 * @brief Error codes used by error reporting and fatal halt handling.
 */
typedef enum {
	ERROR_NONE = 0,                  /**< No recoverable error is currently recorded. */
	ERROR_WATCHDOG_TIMEOUT = 1,      /**< A task health timeout was detected. */
	ERROR_FREERTOS_STACK = 2,        /**< FreeRTOS heap or stack failure occurred. */
	ERROR_PICO_SDK_PANIC = 3,        /**< The Pico SDK invoked the panic handler. */
	ERROR_SCHEDULER_FAILED = 4,      /**< The FreeRTOS scheduler returned unexpectedly. */
	ERROR_RESOURCE_ALLOCATION = 5,   /**< Application resource allocation failed. */
	ERROR_USB_INIT = 6,              /**< USB communication subsystem initialisation failed. */
} error_type_t;

/**
 * @typedef statistics_counter_enum_t
 * @brief Indexes diagnostic and error counters reported to the host.
 */
typedef enum statistics_counter_enum_t {
	QUEUE_SEND_ERROR,                 /**< Input event queue send failures. */
	QUEUE_RECEIVE_ERROR,              /**< Input event queue receive failures. */
	CDC_QUEUE_SEND_ERROR,             /**< CDC transmit queue send failures. */
	DISPLAY_OUT_ERROR,                /**< Display output operation failures. */
	LED_OUT_ERROR,                    /**< LED output operation failures. */
	WATCHDOG_ERROR,                   /**< Watchdog resets recorded across boots. */
	MSG_MALFORMED_ERROR,              /**< Inbound frames with invalid message structure. */
	COBS_DECODE_ERROR,                /**< Empty or invalid COBS frames. */
	RECEIVE_BUFFER_OVERFLOW_ERROR,    /**< Encoded input exceeded the framer capacity. */
	CHECKSUM_ERROR,                   /**< Inbound frame checksum mismatches. */
	BUFFER_OVERFLOW_ERROR,            /**< Packet or output buffer capacity violations. */
	UNKNOWN_CMD_ERROR,                /**< Unsupported board or command identifiers. */
	BYTES_SENT,                       /**< Total encoded bytes accepted for USB transmission. */
	BYTES_RECEIVED,                   /**< Total bytes read from the USB CDC FIFO. */
	RESOURCE_ALLOCATION_ERROR,        /**< Application resource allocation failures. */
	OUTPUT_CONTROLLER_ID_ERROR,       /**< Output payload contains an invalid controller ID. */
	OUTPUT_INIT_ERROR,                /**< Output subsystem initialisation failures. */
	OUTPUT_DRIVER_INIT_ERROR,         /**< Individual output driver initialisation failures. */
	OUTPUT_INVALID_PARAM_ERROR,       /**< Output payload parameter validation failures. */
	INPUT_QUEUE_INIT_ERROR,           /**< Input event queue initialisation failures. */
	INPUT_QUEUE_FULL_ERROR,           /**< Input event queue send timeouts or capacity drops. */
	INPUT_INIT_ERROR,                 /**< Input subsystem initialisation failures. */
	INPUT_HYSTERESIS_SUPPRESSED,      /**< ADC samples suppressed by the hysteresis deadband; diagnostic only. */
	NUM_STATISTICS_COUNTERS           /**< Number of statistics counters. */
} statistics_counter_enum_t;

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
 * @brief Record a recoverable error for diagnostics.
 *
 * @param[in] type Recoverable error type being raised.
 */
void error_management_set_error_state(error_type_t type);

/**
 * @fn void fatal_halt(error_type_t type)
 * @brief Immediately halt execution and display a fatal error pattern.
 *
 * Records the fatal state and error code, then repeatedly displays its LED
 * pattern until the hardware watchdog resets the device.
 *
 * @param[in] type Fatal error code to record and display.
 */
void __attribute__((noreturn)) fatal_halt(error_type_t type);

/**
 * @fn void panic_handler(const char *fmt, ...)
 * @brief Handle a Pico SDK panic by recording a fatal error and halting.
 *
 * @param[in] fmt Format string supplied by the Pico SDK; this handler does not
 *                format or print it.
 * @param[in] ... Variadic values corresponding to @p fmt.
 */
void __attribute__((noreturn)) panic_handler(const char *fmt, ...);

/**
 * @brief Check whether an error type is fatal.
 *
 * @param[in] type Error code to classify.
 *
 * @retval true The type is not classified as a recoverable watchdog or
 *              resource-allocation error.
 * @retval false The type is classified as recoverable.
 */
bool error_management_is_fatal(error_type_t type);

/**
 * @brief Show a blinking error pattern on the error LED.
 *
 * @param[in] error_type The type of error to display.
 */
void show_error_pattern_blocking(error_type_t error_type);

/**
 * @brief Configure the watchdog, error LED, and persistent reset counter.
 *
 * @param[in] timeout_ms Hardware watchdog timeout in milliseconds.
 */
void setup_watchdog_with_error_detection(uint32_t timeout_ms);

/**
 * @brief Check whether a fatal error has been recorded.
 *
 * @return `true` after the fatal halt routine records an error; otherwise
 *         `false`.
 */
bool error_management_has_fatal_error(void);

#endif // ERROR_MANAGEMENT_H
