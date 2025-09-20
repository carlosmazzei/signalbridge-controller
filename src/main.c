/**
 * @file main.c
 * @brief Main application entry point for the Signalbridge Controller firmware.
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "tusb.h"

#include "app_setup.h"
#include "app_tasks.h"
#include "error_management.h"

/**
 * @brief Application entry point initialising hardware and starting FreeRTOS.
 */
int main(void)
{
	setup_watchdog_with_error_detection(5000U);

	if (statistics_is_error_state())
	{
		show_error_for_duration_ms(ERROR_DISPLAY_BEFORE_TENTATIVE_RESTART_MS);

		const uint32_t error_count = watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG];
		statistics_set_counter(WATCHDOG_ERROR, error_count);
		if (error_count > 5U)
		{
			while (true)
			{
				show_error_pattern_blocking(statistics_get_error_type());
				update_watchdog_safe();
			}
		}

		app_tasks_cleanup();
		statistics_clear_error();
	}

	board_init();

	if (!tud_init(BOARD_TUD_RHPORT))
	{
		set_error_state_persistent(ERROR_RESOURCE_ALLOCATION);
	}

	const bool hardware_ready = app_setup_hardware();
	if (!hardware_ready)
	{
		set_error_state_persistent(ERROR_RESOURCE_ALLOCATION);
	}

	const bool tasks_ready = app_tasks_create_all();
	if (!tasks_ready)
	{
		set_error_state_persistent(ERROR_RESOURCE_ALLOCATION);
	}

	vTaskStartScheduler();

	set_error_state_persistent(ERROR_SCHEDULER_FAILED);

	while (true)
	{
		show_error_pattern_blocking(ERROR_SCHEDULER_FAILED);
		update_watchdog_safe();
	}

	return 0;
}
