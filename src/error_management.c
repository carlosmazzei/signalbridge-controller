/**
 * @file error_management.c
 * @brief Error management and LED indication for system errors
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 *
 * This file contains error management functions and LED indication for system errors.
 *
 * @date 2020-2025
 * @copyright
 *   (c) 2020-2025 Carlos Mazzei - All rights reserved.
 *
 */

#include "pico/time.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "FreeRTOS.h"
#include "task.h"
#include "error_management.h"

/**
 * @brief Statistics counters instance (module scope).
 */
static volatile statistics_counters_t statistics_counters = {
	.counters = {0},
	.error_state = false,
	.current_error_type = ERROR_NONE,
	.recovery_state = RECOVERY_STATE_CLEAN
};

static volatile bool error_display_active = false;

static bool error_type_allows_recovery(error_type_t type)
{
	switch (type)
	{
	case ERROR_WATCHDOG_TIMEOUT:
	case ERROR_RESOURCE_ALLOCATION:
		return true;
	default:
		return false;
	}
}

bool error_management_is_recoverable(error_type_t type)
{
	return error_type_allows_recovery(type);
}

void statistics_increment_counter(statistics_counter_enum_t index)
{
	statistics_counters.counters[index]++;

}

void statistics_add_to_counter(statistics_counter_enum_t index, uint32_t value)
{
	statistics_counters.counters[index] += value;
}

void statistics_set_counter(statistics_counter_enum_t index, uint32_t value)
{
	statistics_counters.counters[index] = value;
}

uint32_t statistics_get_counter(statistics_counter_enum_t index)
{
	return statistics_counters.counters[index];
}

void statistics_reset_all_counters(void)
{
	for (uint32_t i = 0; i < NUM_STATISTICS_COUNTERS; i++)
	{
		statistics_counters.counters[i] = 0;
	}
}

bool statistics_is_error_state(void)
{
	return statistics_counters.error_state;
}

error_type_t statistics_get_error_type(void)
{
	return statistics_counters.current_error_type;
}

void statistics_clear_error(void)
{
	statistics_counters.error_state = false;
	statistics_counters.current_error_type = ERROR_NONE;
	statistics_counters.recovery_state = RECOVERY_STATE_CLEAN;
}

void show_error_pattern_blocking(error_type_t error_type)
{
	// Prevent nested error pattern calls
	if (error_display_active)
	{
		return;
	}

	error_display_active = true;
	uint8_t blink_count = (uint8_t)error_type;

	// Show the blink pattern
	for (uint8_t i = 0; i < blink_count; i++) {
		gpio_put(ERROR_LED_PIN, 1);
		busy_wait_ms(BLINK_ON_MS);
		gpio_put(ERROR_LED_PIN, 0);

		if (i < (blink_count - 1))
		{
			// Short pause between blinks (except after last blink)
			busy_wait_ms(BLINK_OFF_MS);
		}
	}

	// Long pause after pattern
	busy_wait_ms(PATTERN_PAUSE_MS);
	error_display_active = false;
}

void show_error_for_duration_ms(uint32_t duration_ms)
{
	// Display the current error pattern repeatedly for a bounded duration,
	// servicing the watchdog on each repetition to avoid resets while in
	// error state. Keep duration below watchdog grace (15s) for tentative
	// recovery to proceed.
	uint32_t start = time_us_32();
	while ((time_us_32() - start) < (duration_ms * 1000u))
	{
		show_error_pattern_blocking(statistics_get_error_type());
		update_watchdog_safe();
	}
}

void error_management_record_recoverable(error_type_t type)
{
	statistics_counters.current_error_type = type;
	statistics_counters.error_state = false;
	statistics_counters.recovery_state = RECOVERY_STATE_CLEAN;

	if (type == ERROR_WATCHDOG_TIMEOUT)
	{
		statistics_increment_counter(WATCHDOG_ERROR);
	}
	else if (type == ERROR_RESOURCE_ALLOCATION)
	{
		statistics_increment_counter(RECOVERY_HEAP_ERROR);
	}
}

void error_management_record_fatal(error_type_t type)
{
	statistics_counters.current_error_type = type;
	statistics_counters.error_state = false;
	statistics_counters.recovery_state = RECOVERY_STATE_FAILED;
}

void setup_watchdog_with_error_detection(uint32_t timeout_ms)
{
	// Initialize LED
	gpio_init(ERROR_LED_PIN);
	gpio_set_dir(ERROR_LED_PIN, GPIO_OUT);
	gpio_put(ERROR_LED_PIN, 0);

	uint32_t watchdog_resets = watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG];
	if (watchdog_caused_reboot())
	{
		watchdog_resets++;
	}
	watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = watchdog_resets;
	statistics_set_counter(WATCHDOG_ERROR, watchdog_resets);

	// Enable watchdog
	watchdog_enable(timeout_ms, 1);
}

void update_watchdog_safe(void)
{
	watchdog_update();
}

recovery_state_t error_management_get_recovery_state(void)
{
	return statistics_counters.recovery_state;
}

void error_management_graceful_reboot(error_type_t error_type, uint32_t delay_ms)
{
	// MISRA D1: Essential for graceful shutdown functionality
	const uint32_t max_delay_ms = 5000U;  // Maximum delay before reboot
	uint32_t actual_delay = delay_ms;

	// Clamp delay to safe maximum
	if (actual_delay > max_delay_ms)
	{
		actual_delay = max_delay_ms;
	}

	// Step 1: Save error state persistently
	error_management_record_recoverable(error_type);

	// Step 2: Set recovery state to indicate controlled reboot
	statistics_counters.recovery_state = RECOVERY_STATE_FAILED;

	// Step 3: Show brief error indication
	show_error_pattern_blocking(error_type);

	// Step 4: Allow time for final operations
	if (actual_delay > 0U)
	{
		uint32_t start_time = time_us_32();
		uint32_t target_delay_us = actual_delay * 1000U;

		// Continue updating watchdog during graceful shutdown period
		while ((time_us_32() - start_time) < target_delay_us)
		{
			watchdog_update();
			busy_wait_ms(100);  // Brief intervals for responsiveness
		}
	}

	// Step 5: Stop watchdog updates to trigger reset
	// The watchdog will reset the system after its timeout period
	uint32_t final_start = time_us_32();
	const uint32_t watchdog_timeout_us = 5000U * 1000U;  // 5 second watchdog timeout
	const uint32_t pattern_duration_us = (uint32_t)error_type * (BLINK_ON_MS + BLINK_OFF_MS) * 1000U + PATTERN_PAUSE_MS * 1000U;

	while (true)
	{
		uint32_t elapsed_us = time_us_32() - final_start;
		uint32_t remaining_us = watchdog_timeout_us > elapsed_us ? watchdog_timeout_us - elapsed_us : 0U;

		// Only show complete pattern if enough time remains
		if (remaining_us > pattern_duration_us)
		{
			show_error_pattern_blocking(error_type);
		}
		else
		{
			// Not enough time for complete pattern - show continuous blink
			gpio_put(ERROR_LED_PIN, 1);
			busy_wait_ms(100);
			gpio_put(ERROR_LED_PIN, 0);
			busy_wait_ms(100);
		}
		// Deliberately not updating watchdog to force reset
	}
}

/**
 * @brief Panic handler for critical errors.
 */
void __attribute__((noreturn)) panic_handler(const char *fmt, ...)
{
	(void)fmt;

	// Disable all interrupts using Pico SDK
	(void)save_and_disable_interrupts();
	fatal_halt(ERROR_PICO_SDK_PANIC);
}

/**
 * @brief Fatal halt helper that records error and shows pattern forever.
 */
void __attribute__((noreturn)) fatal_halt(error_type_t type)
{
	error_management_record_fatal(type);
	while (true)
	{
		show_error_pattern_blocking(type);
		update_watchdog_safe();
	}
}
