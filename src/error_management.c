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
        .current_error_type = ERROR_NONE
};

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
}

void show_error_pattern_blocking(error_type_t error_type)
{
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
}

/**
 * @brief Check the previous error state from the watchdog registers.
 *
 */
static void check_previous_error_state(void)
{
	uint32_t boot_magic = watchdog_hw->scratch[WATCHDOG_BOOT_MAGIC_REG];
	uint32_t error_type = watchdog_hw->scratch[WATCHDOG_ERROR_TYPE_REG];

	// Only check for errors if we have a valid magic value
	if (ERROR_MAGIC_VALUE == boot_magic) // Only check if explicitly set by set_error_state_persistent()
	{
		// This was definitely an error boot
		if (error_type == ERROR_NONE)
		{
			statistics_counters.current_error_type = ERROR_WATCHDOG_TIMEOUT;
		}
		else if (error_type <= ERROR_RESOURCE_ALLOCATION)  // Remove >= 0 check
		{
			statistics_counters.current_error_type = (error_type_t)error_type;
		}
		else
		{
			statistics_counters.current_error_type = ERROR_WATCHDOG_TIMEOUT;
		}

		statistics_counters.error_state = true;

		uint32_t count = watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG];
		watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = count + 1;
	}
	else
	{
		// Either first boot or clean boot - treat as clean
		watchdog_hw->scratch[WATCHDOG_ERROR_TYPE_REG] = ERROR_NONE;
		watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = 0;
		statistics_counters.error_state = false;
		statistics_counters.current_error_type = ERROR_NONE;
	}

	// Mark as clean boot for next time
	watchdog_hw->scratch[WATCHDOG_BOOT_MAGIC_REG] = CLEAN_BOOT_MAGIC;
}

void set_error_state_persistent(error_type_t type)
{
	watchdog_hw->scratch[WATCHDOG_ERROR_TYPE_REG] = type;
	watchdog_hw->scratch[WATCHDOG_BOOT_MAGIC_REG] = ERROR_MAGIC_VALUE;

	statistics_counters.current_error_type = type;
	statistics_counters.error_state = true;

	// Increment error count
	uint32_t count = watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG];
	watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = count + 1;
}

void setup_watchdog_with_error_detection(uint32_t timeout_ms)
{
	// Initialize LED
	gpio_init(ERROR_LED_PIN);
	gpio_set_dir(ERROR_LED_PIN, GPIO_OUT);
	gpio_put(ERROR_LED_PIN, 0);

	// Check previous error state
	check_previous_error_state();

	// Enable watchdog
	watchdog_enable(timeout_ms, 1);
}

void update_watchdog_safe(void)
{
	static uint32_t error_start_time = 0;

	if (false == statistics_counters.error_state)
	{
		watchdog_update();
		error_start_time = 0;
	}
	else
	{
		// In error state - show error for limited time then reset
		if (0 == error_start_time)
		{
			error_start_time = time_us_32();
		}

		// Keep alive for 15 seconds while showing error
		if ((time_us_32() - error_start_time) < 15000000)
		{
			watchdog_update();
		}
		else
		{
			set_error_state_persistent(ERROR_WATCHDOG_TIMEOUT);
			// Stop updating - let watchdog reset
		}
	}
}

/**
 * @brief Panic handler for critical errors.
 */
void __attribute__((noreturn)) panic_handler(const char *fmt, ...)
{
	(void)fmt;

	set_error_state_persistent(ERROR_PICO_SDK_PANIC);

	// Disable all interrupts using Pico SDK
	(void)save_and_disable_interrupts();

	// Show error pattern forever using busy wait
	while (true) {
		show_error_pattern_blocking(ERROR_PICO_SDK_PANIC);
	}
}