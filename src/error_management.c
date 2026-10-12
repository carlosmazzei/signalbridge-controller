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
#include "atomic_u32.h"

/**
 * @brief Statistics counters instance (module scope).
 */
static _Atomic uint32_t counters[NUM_STATISTICS_COUNTERS];
static _Atomic uint32_t current_error_type;
static _Atomic uint32_t fatal_error;

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
	if ((uint32_t)index < (uint32_t)NUM_STATISTICS_COUNTERS)
	{
		atomic_u32_add(&counters[index], 1U);
	}
}

void statistics_add_to_counter(statistics_counter_enum_t index, uint32_t value)
{
	if ((uint32_t)index < (uint32_t)NUM_STATISTICS_COUNTERS)
	{
		atomic_u32_add(&counters[index], value);
	}
}

void statistics_set_counter(statistics_counter_enum_t index, uint32_t value)
{
	if ((uint32_t)index < (uint32_t)NUM_STATISTICS_COUNTERS)
	{
		atomic_u32_store(&counters[index], value);
	}
}

uint32_t statistics_get_counter(statistics_counter_enum_t index)
{
	uint32_t value = 0U;

	if ((uint32_t)index < (uint32_t)NUM_STATISTICS_COUNTERS)
	{
		value = atomic_u32_load(&counters[index]);
	}
	return value;
}

void statistics_reset_all_counters(void)
{
	for (uint32_t i = 0U; i < (uint32_t)NUM_STATISTICS_COUNTERS; i++)
	{
		atomic_u32_store(&counters[i], 0U);
	}
}

bool statistics_is_error_state(void)
{
	return atomic_u32_load(&current_error_type) != (uint32_t)ERROR_NONE;
}

error_type_t statistics_get_error_type(void)
{
	return (error_type_t)atomic_u32_load(&current_error_type);
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
	atomic_u32_store(&current_error_type, (uint32_t)type);

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
 *
 * Installed as the Pico SDK panic() target through PICO_PANIC_FUNCTION.
 */
// MISRA Deviation D2 (Rule 8.7): hook referenced by lib/ code
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
	atomic_u32_store(&fatal_error, 1U);
	watchdog_hw->scratch[WATCHDOG_ERROR_LAST_TYPE_REG] = (uint32_t)type;
	while (true)
	{
		show_error_pattern_blocking(type);
	}
}

bool error_management_has_fatal_error(void)
{
	return atomic_u32_load(&fatal_error) != 0U;
}
