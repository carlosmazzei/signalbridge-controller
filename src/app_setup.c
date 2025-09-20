/**
 * @file app_setup.c
 * @brief Hardware and peripheral initialisation helpers.
 */

#include "app_setup.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "pico/stdlib.h"

#include "app_config.h"
#include "app_context.h"
#include "data_event.h"
#include "error_management.h"
#include "inputs.h"
#include "outputs.h"

/** @copydoc app_setup_hardware */
bool app_setup_hardware(void)
{
        bool success = true;

        app_context_reset_queues();
        app_context_reset_line_state();

        stdio_init_all();

        const output_result_t output_status = output_init();
        if (output_status != OUTPUT_OK)
        {
                success = false;
        }

        QueueHandle_t data_queue = xQueueCreate(DATA_EVENT_QUEUE_SIZE, sizeof(data_events_t));
        if (NULL == data_queue)
        {
                success = false;
        }
        app_context_set_data_event_queue(data_queue);

        const input_config_t config = {
                .columns                  = 8,
                .rows                     = 8,
                .key_settling_time_ms     = 20,
                .input_event_queue        = data_queue,
                .adc_channels             = 16,
                .adc_settling_time_ms     = 100,
                .encoder_settling_time_ms = 10,
                .encoder_mask             = { [7] = true }
        };

        const input_result_t input_status = input_init(&config);
        if (input_status != INPUT_OK)
        {
                success = false;
        }

        statistics_reset_all_counters();
        app_context_reset_task_props();

        return success;
}
