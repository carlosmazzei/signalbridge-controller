/**
 * @file app_context.c
 * @brief Shared application context (queues, task properties, line state).
 */

#include "app_context.h"

#include <stdint.h>

/** @brief Singleton instance storing shared resources.
 *
 */
static app_context_t s_app_context = {
	.encoded_reception_queue = NULL,
	.data_event_queue        = NULL,
	.cdc_transmit_queue      = NULL,
	.task_props              = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}},
	.cdc_rts                 = ATOMIC_VAR_INIT(false),
	.cdc_dtr                 = ATOMIC_VAR_INIT(false)
};

bool app_context_is_cdc_ready(void)
{
	const app_context_t *const context = app_context_get();
	bool dtr = atomic_load_explicit(&context->cdc_dtr, memory_order_acquire);
	bool rts = atomic_load_explicit(&context->cdc_rts, memory_order_acquire);
	return (dtr && rts);
}

task_props_t *app_context_task_props(task_enum_t task_id)
{
	return &app_context_get()->task_props[task_id];
}

QueueHandle_t app_context_get_encoded_queue(void)
{
	return app_context_get()->encoded_reception_queue;
}

void app_context_set_encoded_queue(QueueHandle_t queue)
{
	app_context_get()->encoded_reception_queue = queue;
}

QueueHandle_t app_context_get_data_event_queue(void)
{
	return app_context_get()->data_event_queue;
}

void app_context_set_data_event_queue(QueueHandle_t queue)
{
	app_context_get()->data_event_queue = queue;
}

QueueHandle_t app_context_get_cdc_transmit_queue(void)
{
	return app_context_get()->cdc_transmit_queue;
}

void app_context_set_cdc_transmit_queue(QueueHandle_t queue)
{
	app_context_get()->cdc_transmit_queue = queue;
}

app_context_t *app_context_get(void)
{
	return &s_app_context;
}

void app_context_reset_task_props(void)
{
	for (uint32_t i = 0; i < (uint32_t)NUM_TASKS; i++)
	{
		s_app_context.task_props[i].high_watermark = 0U;
		s_app_context.task_props[i].task_handle = NULL;
	}
}

void app_context_reset_queues(void)
{
	if (s_app_context.encoded_reception_queue != NULL)
	{
		vQueueDelete(s_app_context.encoded_reception_queue);
		s_app_context.encoded_reception_queue = NULL;
	}
	if (s_app_context.data_event_queue != NULL)
	{
		vQueueDelete(s_app_context.data_event_queue);
		s_app_context.data_event_queue = NULL;
	}
	if (s_app_context.cdc_transmit_queue != NULL)
	{
		vQueueDelete(s_app_context.cdc_transmit_queue);
		s_app_context.cdc_transmit_queue = NULL;
	}
}

void app_context_set_line_state(bool dtr, bool rts)
{
	atomic_store_explicit(&s_app_context.cdc_dtr, dtr, memory_order_release);
	atomic_store_explicit(&s_app_context.cdc_rts, rts, memory_order_release);
}

void app_context_reset_line_state(void)
{
	app_context_set_line_state(false, false);
}
