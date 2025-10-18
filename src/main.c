/**
 * @file main.c
 * @brief Main application entry point for the Signalbridge Controller firmware.
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include <hardware/watchdog.h>
#include <pico/stdlib.h>

#include "bsp/board.h"
#include "tusb.h"

#include "app_context.h"
#include "app_inputs.h"
#include "app_tasks.h"
#include "app_outputs.h"
#include "error_management.h"

/**
 * @brief Application entry point initialising hardware and starting FreeRTOS.
 */
int main(void)
{
	board_init();
	stdio_init_all();

    app_tasks_cleanup_application();
	app_context_reset_queues();
	app_context_reset_line_state();
	app_context_reset_task_props();
	statistics_reset_all_counters();

    setup_watchdog_with_error_detection(WATCHDOG_GRACE_PERIOD_MS);

	// Initialize TinyUSB
	if (!tud_init(BOARD_TUD_RHPORT))
	{
		fatal_halt(ERROR_USB_INIT);
	}

	// Initialize communication subsystem
	if (!app_tasks_create_comm())
	{
		fatal_halt(ERROR_USB_INIT);
	}

	// Initialize outputs
	const output_result_t output_status = output_init();
	if (output_status != OUTPUT_OK)
	{
		statistics_increment_counter(OUTPUT_INIT_ERROR);
	}

	// Initialize inputs
	const input_result_t input_status = input_init();
    if (input_status != INPUT_OK)
    {
        statistics_increment_counter(INPUT_INIT_ERROR);
    }

    // Initialize application tasks
	const bool tasks_ready = app_tasks_create_application();
	if (!tasks_ready)
	{
		error_management_record_recoverable(ERROR_RESOURCE_ALLOCATION);
	}

    // Initialize task scheduler
	vTaskStartScheduler();
	fatal_halt(ERROR_SCHEDULER_FAILED);

	return 0;
}
