/**
 * @file app_context.c
 * @brief Shared application context (queues, task properties, line state).
 */

#include "app_context.h"

#include <stdint.h>

/** @brief Singleton instance storing shared resources.
 *
 */
static app_context_t s_app_context;

bool app_context_is_cdc_ready(void)
{
	return (atomic_load_explicit(&s_app_context.cdc_state, memory_order_acquire) & 1U) != 0U;
}

uint32_t app_context_cdc_session(void)
{
	return atomic_load_explicit(&s_app_context.cdc_state, memory_order_acquire) >> 1U;
}

task_props_t *app_context_task_props(task_enum_t task_id)
{
	return ((unsigned)task_id < NUM_TASKS) ? &app_context_get()->task_props[task_id] : NULL;
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
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
		return;
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
	uint32_t old = atomic_load_explicit(&s_app_context.cdc_state, memory_order_relaxed);
	uint32_t ready = (dtr && rts) ? 1U : 0U;
	while ((old & 1U) != ready)
	{
		uint32_t next = ((old + 2U) & ~1U) | ready;
		bool changed = atomic_compare_exchange_weak_explicit(&s_app_context.cdc_state, &old, next,
		                                                     memory_order_release, memory_order_relaxed);
		if (changed)
			break;
	}
}

void app_context_reset_line_state(void)
{
	app_context_set_line_state(false, false);
}
