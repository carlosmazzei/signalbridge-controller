/**
 * @file app_tasks.c
 * @brief Creation and teardown of application FreeRTOS tasks.
 */

#include "app_tasks.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "pico/stdlib.h"

#include "tusb.h"

#include "cobs.h"
#include "app_comm.h"
#include "app_config.h"
#include "app_context.h"
#include "data_event.h"
#include "error_management.h"
#include "inputs.h"

/**
 * @brief Helper to create a task and set its core affinity.
 *
 * @param[in] function      Task entry function.
 * @param[in] name          Task name.
 * @param[in] stack_size    Task stack size.
 * @param[in] params        Parameters passed to the task.
 * @param[in] priority      Task priority.
 * @param[in] task_id       Identifier used to store the task properties.
 * @param[in] affinity_mask Core affinity mask for the task.
 * @return `true` when the task was created successfully.
 */
static inline bool create_task_with_affinity(TaskFunction_t function,
                                             const char *name,
                                             configSTACK_DEPTH_TYPE stack_size,
                                             void *params,
                                             UBaseType_t priority,
                                             task_enum_t task_id,
                                             UBaseType_t affinity_mask)
{
	task_props_t *const props = app_context_task_props(task_id);
	const BaseType_t result = xTaskCreate(function,
	                                      name,
	                                      stack_size,
	                                      params,
	                                      priority,
	                                      &props->task_handle);
	if (pdPASS == result)
	{
		vTaskCoreAffinitySet(props->task_handle, affinity_mask);
		return true;
	}

	props->task_handle = NULL;
	set_error_state_persistent(ERROR_FREERTOS_STACK);
	return false;
}

/**
 * @brief Helper to create a queue and flag an error when allocation fails.
 *
 * @param[in] length    Queue length.
 * @param[in] item_size Size of each item.
 * @return The queue handle or `NULL` when allocation fails.
 */
static inline QueueHandle_t create_queue_or_flag(UBaseType_t length, UBaseType_t item_size)
{
	QueueHandle_t queue = xQueueCreate(length, item_size);
	if (NULL == queue)
	{
		set_error_state_persistent(ERROR_FREERTOS_STACK);
	}
	return queue;
}

static void uart_event_task(void *pvParameters);
static void cdc_task(void *pvParameters);
static void decode_reception_task(void *pvParameters);
static void process_outbound_task(void *pvParameters);
static void cdc_write_task(void *pvParameters);
static void led_status_task(void *pvParameters);

bool app_tasks_create_all(void)
{
	bool success = true;

	task_props_t *cdc_props = app_context_task_props(CDC_TASK);
	if (!create_task_with_affinity(cdc_task,
	                               "cdc_task",
	                               CDC_STACK_SIZE,
	                               (void *)cdc_props,
	                               mainCDC_TASK_PRIORITY,
	                               CDC_TASK,
	                               CDC_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	QueueHandle_t encoded_queue = create_queue_or_flag(ENCODED_QUEUE_SIZE, sizeof(uint8_t));
	app_context_set_encoded_queue(encoded_queue);
	if (encoded_queue != NULL)
	{
		if (!create_task_with_affinity(uart_event_task,
		                               "uart_event_task",
		                               UART_EVENT_STACK_SIZE,
		                               (void *)app_context_task_props(UART_EVENT_TASK),
		                               mainUART_TASK_PRIORITY,
		                               UART_EVENT_TASK,
		                               UART_EVENT_TASK_CORE_AFFINITY))
		{
			success = false;
		}

		if (!create_task_with_affinity(decode_reception_task,
		                               "decode_reception_task",
		                               DECODE_RECEPTION_STACK_SIZE,
		                               (void *)app_context_task_props(DECODE_RECEPTION_TASK),
		                               mainDECODE_TASK_PRIORITY,
		                               DECODE_RECEPTION_TASK,
		                               DECODE_RECEPTION_TASK_CORE_AFFINITY))
		{
			success = false;
		}
	}
	else
	{
		success = false;
	}

	QueueHandle_t transmit_queue = create_queue_or_flag(CDC_TRANSMIT_QUEUE_SIZE, sizeof(cdc_packet_t));
	app_context_set_cdc_transmit_queue(transmit_queue);
	if (transmit_queue != NULL)
	{
		if (!create_task_with_affinity(cdc_write_task,
		                               "cdc_write_task",
		                               CDC_STACK_SIZE,
		                               (void *)app_context_task_props(CDC_WRITE_TASK),
		                               mainCDC_TASK_PRIORITY,
		                               CDC_WRITE_TASK,
		                               CDC_TASK_CORE_AFFINITY))
		{
			success = false;
		}
	}
	else
	{
		success = false;
	}

	if (!create_task_with_affinity(process_outbound_task,
	                               "process_outbound_task",
	                               PROCESS_OUTBOUND_STACK_SIZE,
	                               (void *)app_context_task_props(PROCESS_OUTBOUND_TASK),
	                               mainPROCESS_QUEUE_TASK_PRIORITY,
	                               PROCESS_OUTBOUND_TASK,
	                               PROCESS_OUTBOUND_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	if (!create_task_with_affinity(adc_read_task,
	                               "adc_read_task",
	                               ADC_READ_STACK_SIZE,
	                               (void *)app_context_task_props(ADC_READ_TASK),
	                               mainADC_TASK_PRIORITY,
	                               ADC_READ_TASK,
	                               ADC_READ_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	if (!create_task_with_affinity(keypad_task,
	                               "keypad_task",
	                               KEYPAD_STACK_SIZE,
	                               (void *)app_context_task_props(KEYPAD_TASK),
	                               mainKEY_TASK_PRIORITY,
	                               KEYPAD_TASK,
	                               KEYPAD_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	if (!create_task_with_affinity(encoder_read_task,
	                               "encoder_task",
	                               ENCODER_READ_STACK_SIZE,
	                               (void *)app_context_task_props(ENCODER_READ_TASK),
	                               mainENCODER_TASK_PRIORITY,
	                               ENCODER_READ_TASK,
	                               ENCODER_READ_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	if (!create_task_with_affinity(led_status_task,
	                               "led_status_task",
	                               LED_STATUS_STACK_SIZE,
	                               (void *)app_context_task_props(LED_STATUS_TASK),
	                               mainLED_STATUS_TASK_PRIORITY,
	                               LED_STATUS_TASK,
	                               LED_STATUS_TASK_CORE_AFFINITY))
	{
		success = false;
	}

	return success;
}

void app_tasks_cleanup(void)
{
	QueueHandle_t queue = app_context_get_encoded_queue();
	if (queue != NULL)
	{
		vQueueDelete(queue);
	}

	queue = app_context_get_data_event_queue();
	if (queue != NULL)
	{
		vQueueDelete(queue);
	}

	queue = app_context_get_cdc_transmit_queue();
	if (queue != NULL)
	{
		vQueueDelete(queue);
	}

	app_context_reset_queues();
	app_context_reset_line_state();

	for (uint32_t id = 0; id < (uint32_t)NUM_TASKS; id++)
	{
		task_props_t *props = app_context_task_props((task_enum_t)id);
		if (props->task_handle != NULL)
		{
			vTaskDelete(props->task_handle);
			props->task_handle = NULL;
		}
		props->high_watermark = 0U;
	}
}

/**
 * @brief Task that reads raw CDC bytes and enqueues them for decoding.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void uart_event_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;
	uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];

	for (;;)
	{
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		update_watchdog_safe();

		if (!tud_cdc_n_available(0))
		{
			taskYIELD();
			continue;
		}

		const uint32_t count = tud_cdc_n_read(0, receive_buffer, sizeof(receive_buffer));
		statistics_add_to_counter(BYTES_RECEIVED, count);

		QueueHandle_t queue = app_context_get_encoded_queue();
		for (uint32_t i = 0; (i < count) && (i < MAX_ENCODED_BUFFER_SIZE); i++)
		{
			if ((NULL == queue) || (xQueueSend(queue, &receive_buffer[i], pdMS_TO_TICKS(5)) != pdTRUE))
			{
				statistics_increment_counter(QUEUE_SEND_ERROR);
			}
			update_watchdog_safe();
		}
	}
}

/**
 * @brief TinyUSB device task responsible for polling the USB stack.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void cdc_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;
	for (;;)
	{
		tud_task();
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		update_watchdog_safe();
		taskYIELD();
	}
}

/**
 * @brief Task that decodes COBS-framed packets received from the host.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void decode_reception_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;
	uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];
	size_t receive_buffer_index = 0U;

	for (;;)
	{
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		update_watchdog_safe();

		QueueHandle_t queue = app_context_get_encoded_queue();
		if (NULL == queue)
		{
			vTaskDelay(pdMS_TO_TICKS(5));
			continue;
		}

		uint8_t data = 0U;
		if (pdFALSE == xQueueReceive(queue, &data, portMAX_DELAY))
		{
			statistics_increment_counter(QUEUE_RECEIVE_ERROR);
			continue;
		}

		if (PACKET_MARKER == data)
		{
			if (0U == receive_buffer_index)
			{
				statistics_increment_counter(COBS_DECODE_ERROR);
				receive_buffer_index = 0U;
				continue;
			}

			uint8_t decode_buffer[MAX_ENCODED_BUFFER_SIZE];
			const size_t num_decoded = cobs_decode(receive_buffer, receive_buffer_index, decode_buffer);
			receive_buffer_index = 0U;

			if (num_decoded > 0U)
			{
				app_comm_process_inbound(decode_buffer, num_decoded);
			}
			else
			{
				statistics_increment_counter(COBS_DECODE_ERROR);
			}
		}
		else if (receive_buffer_index < (MAX_ENCODED_BUFFER_SIZE - 1U))
		{
			receive_buffer[receive_buffer_index] = data;
			receive_buffer_index++;
		}
		else
		{
			statistics_increment_counter(RECEIVE_BUFFER_OVERFLOW_ERROR);
			receive_buffer_index = 0U;
		}
	}
}

/**
 * @brief Task that sends queued input events to the host.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void process_outbound_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;

	for (;;)
	{
		QueueHandle_t data_queue = app_context_get_data_event_queue();
		if (NULL == data_queue)
		{
			vTaskDelay(pdMS_TO_TICKS(5));
			continue;
		}

		data_events_t data_event;
		BaseType_t result = xQueueReceive(data_queue, (void *)&data_event, portMAX_DELAY);
		if (pdPASS == result)
		{
			app_comm_send_packet(BOARD_ID, data_event.command, data_event.data, data_event.data_length);
		}
		else if (errQUEUE_EMPTY == result)
		{
			// No action required.
		}
		else
		{
			statistics_increment_counter(QUEUE_RECEIVE_ERROR);
		}

		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		update_watchdog_safe();
	}
}

/**
 * @brief Task that transmits encoded packets over USB CDC.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void cdc_write_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;
	cdc_packet_t packet;

	for (;;)
	{
		QueueHandle_t queue = app_context_get_cdc_transmit_queue();
		if ((queue != NULL) && (pdTRUE == xQueueReceive(queue, &packet, portMAX_DELAY)))
		{
			while (!app_context_is_cdc_ready())
			{
				vTaskDelay(pdMS_TO_TICKS(5));
			}

			size_t total_written = 0U;
			while (total_written < packet.length)
			{
				const uint32_t available = tud_cdc_n_write_available(0);
				const uint32_t remaining = (uint32_t)packet.length - (uint32_t)total_written;
				const uint32_t to_write = (available < remaining) ? available : remaining;

				if (to_write > 0U)
				{
					tud_cdc_n_write(0, &packet.data[total_written], to_write);
					total_written += to_write;
				}

				tud_task();
				taskYIELD();
			}

			statistics_add_to_counter(BYTES_SENT, (uint32_t)total_written);
			tud_cdc_write_flush();
		}
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		update_watchdog_safe();
	}
}

/**
 * @brief Task that drives the status LED based on system state.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void led_status_task(void *pvParameters)
{
	task_props_t *task_prop = (task_props_t *)pvParameters;

	for (;;)
	{
		if (statistics_is_error_state())
		{
			const uint8_t blink_count = (uint8_t)statistics_get_error_type();

			for (uint8_t i = 0U; i < blink_count; i++)
			{
				gpio_put(ERROR_LED_PIN, 1);
				vTaskDelay(pdMS_TO_TICKS(BLINK_ON_MS));
				gpio_put(ERROR_LED_PIN, 0);

				if (i + 1U < blink_count)
				{
					vTaskDelay(pdMS_TO_TICKS(BLINK_OFF_MS));
				}
			}

			vTaskDelay(pdMS_TO_TICKS(PATTERN_PAUSE_MS));
		}
		else
		{
			if (tud_cdc_n_connected(0))
			{
				gpio_put(ERROR_LED_PIN, 1);
			}
			else
			{
				gpio_put(ERROR_LED_PIN, 0);
			}

			vTaskDelay(pdMS_TO_TICKS(100));
		}

		update_watchdog_safe();
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
	}
}

/**
 * @brief FreeRTOS hook that retrieves the current runtime counter in microseconds.
 *
 * @return Monotonic runtime value used by the kernel statistics module.
 */
uint32_t ulPortGetRunTime(void)
{
	return time_us_32();
}
