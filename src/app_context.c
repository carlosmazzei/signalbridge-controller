/**
 * @file app_context.c
 * @brief Shared application context (queues, task properties, line state).
 */

#include "app_context.h"

#include <stdint.h>

/** @brief Singleton instance storing shared resources. */
static app_context_t app_context = {
        .encoded_reception_queue = NULL,
        .data_event_queue        = NULL,
        .cdc_transmit_queue      = NULL,
        .task_props              = {0},
        .cdc_rts                 = false,
        .cdc_dtr                 = false
};

app_context_t *app_context_get(void)
{
        return &app_context;
}

void app_context_reset_task_props(void)
{
        for (uint32_t i = 0; i < (uint32_t)NUM_TASKS; i++)
        {
                app_context.task_props[i].high_watermark = 0U;
                app_context.task_props[i].task_handle = NULL;
        }
}

void app_context_reset_queues(void)
{
        app_context.encoded_reception_queue = NULL;
        app_context.data_event_queue = NULL;
        app_context.cdc_transmit_queue = NULL;
}

void app_context_set_line_state(bool dtr, bool rts)
{
        app_context.cdc_dtr = dtr;
        app_context.cdc_rts = rts;
}

void app_context_reset_line_state(void)
{
        app_context_set_line_state(false, false);
}
