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

#include <pico/time.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/watchdog.h>

#include "FreeRTOS.h"
#include "task.h"
#include "error_management.h"
#include <stdatomic.h>

/**
 * @brief Statistics counters instance (module scope).
 */
static _Atomic uint32_t counters[NUM_STATISTICS_COUNTERS];
static _Atomic error_type_t current_error_type;
static atomic_bool fatal_error;

static bool error_type_non_fatal(error_type_t type)
{
	bool result = false;
	switch (type)
	{
	case ERROR_WATCHDOG_TIMEOUT:
	case ERROR_RESOURCE_ALLOCATION:
		result = true;
		break;
	default:
		result = false;
		break;
	}
	return result;
}

bool error_management_is_fatal(error_type_t type)
{
	return !error_type_non_fatal(type);
}

void statistics_increment_counter(statistics_counter_enum_t index)
{
	if ((unsigned)index < NUM_STATISTICS_COUNTERS)
		(void)atomic_fetch_add_explicit(&counters[index], 1U, memory_order_relaxed);

}

void statistics_add_to_counter(statistics_counter_enum_t index, uint32_t value)
{
	if ((unsigned)index < NUM_STATISTICS_COUNTERS)
		(void)atomic_fetch_add_explicit(&counters[index], value, memory_order_relaxed);
}

void statistics_set_counter(statistics_counter_enum_t index, uint32_t value)
{
	if ((unsigned)index < NUM_STATISTICS_COUNTERS)
		atomic_store_explicit(&counters[index], value, memory_order_relaxed);
}

uint32_t statistics_get_counter(statistics_counter_enum_t index)
{
	return ((unsigned)index < NUM_STATISTICS_COUNTERS)
	? atomic_load_explicit(&counters[index], memory_order_relaxed) : 0U;
}

void statistics_reset_all_counters(void)
{
	for (uint32_t i = 0; i < (uint32_t)NUM_STATISTICS_COUNTERS; i++)
	{
		atomic_store_explicit(&counters[i], 0U, memory_order_relaxed);
	}
}

bool statistics_is_error_state(void)
{
	return atomic_load_explicit(&current_error_type, memory_order_relaxed) != ERROR_NONE;
}

error_type_t statistics_get_error_type(void)
{
	return atomic_load_explicit(&current_error_type, memory_order_relaxed);
}

void show_error_pattern_blocking(error_type_t error_type)
{
	static volatile bool error_display_active = false;

	// Prevent nested error pattern calls
	if (!error_display_active)
	{
		error_display_active = true;
		uint8_t blink_count = (uint8_t)error_type;

		// Show the blink pattern
		for (uint8_t i = 0; i < blink_count; i++) {
			gpio_put(ERROR_LED_PIN, 1);
			busy_wait_ms(BLINK_ON_MS);
			gpio_put(ERROR_LED_PIN, 0);

			if (i < (blink_count - 1U))
			{
				// Short pause between blinks (except after last blink)
				busy_wait_ms(BLINK_OFF_MS);
			}
		}

		// Long pause after pattern
		busy_wait_ms(PATTERN_PAUSE_MS);
		error_display_active = false;
	}
}

void error_management_set_error_state(error_type_t type)
{
	atomic_store_explicit(&current_error_type, type, memory_order_relaxed);

	switch (type)
	{
	case ERROR_WATCHDOG_TIMEOUT:
		statistics_increment_counter(WATCHDOG_ERROR);
		break;
	case ERROR_RESOURCE_ALLOCATION:
		statistics_increment_counter(RESOURCE_ALLOCATION_ERROR);
		break;
	default:
		statistics_increment_counter(RESOURCE_ALLOCATION_ERROR);
		break;
	}
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
	// check for overflow
	if (0xFFFFFFFFU == watchdog_resets)
	{
		watchdog_resets = 0;
	}
	watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = watchdog_resets;
	statistics_set_counter(WATCHDOG_ERROR, watchdog_resets);

	// Enable watchdog
	watchdog_enable(timeout_ms, 1);
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
 * @brief Record a fatal error and blink until the hardware watchdog resets.
 */
void __attribute__((noreturn)) fatal_halt(error_type_t type)
{
	atomic_store_explicit(&fatal_error, true, memory_order_release);
	watchdog_hw->scratch[WATCHDOG_ERROR_LAST_TYPE_REG] = (uint32_t)type;
	while (true)
	{
		show_error_pattern_blocking(type);
	}
}

bool error_management_has_fatal_error(void)
{
	return atomic_load_explicit(&fatal_error, memory_order_acquire);
}
