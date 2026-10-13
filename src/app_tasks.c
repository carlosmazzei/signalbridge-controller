/**
 * @file app_tasks.c
 * @brief Creation and teardown of application FreeRTOS tasks.
 */

#include "app_tasks.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <pico/stdlib.h>
#include <hardware/watchdog.h>

#include "tusb.h"

#include <stddef.h>

#include "cobs.h"
#include "app_comm.h"
#include "app_config.h"
#include "app_context.h"
#include "app_inputs.h"
#include "data_event.h"
#include "encoded_framer.h"
#include "error_management.h"
#include "input_delivery.h"
#include "task_health.h"
#include "device/dcd.h"

static void uart_event_task(void *pvParameters);
static void cdc_task(void *pvParameters);
static void decode_reception_task(void *pvParameters);
static void process_outbound_task(void *pvParameters);
static void cdc_write_task(void *pvParameters);
static void led_status_task(void *pvParameters);
static void cleanup_comm_subsystem(void);

/**
 * @brief Check TinyUSB endpoint readiness and host line-state readiness.
 *
 * @return `true` when CDC is ready for transfers; otherwise `false`.
 */
static bool usb_ready(void)
{
	return tud_cdc_n_ready(0) && app_context_is_cdc_ready();
}

/**
 * @brief Reset CDC line state on USB bus reset or unplug events.
 *
 * @param[in] rhport Root-hub port reported by TinyUSB.
 * @param[in] eventid Device-controller event identifier.
 * @param[in] in_isr Whether TinyUSB reports that the event originated in an ISR.
 *
 * Bus reset and unplug can occur without a line-state callback. The hook only
 * updates atomic session state and is safe when invoked in interrupt context.
 */
// MISRA Deviation D2 (Rule 8.7): hook referenced by lib/ code
void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr)
{
	(void)rhport;
	(void)in_isr;
	if ((eventid == (uint32_t)DCD_EVENT_BUS_RESET) || (eventid == (uint32_t)DCD_EVENT_UNPLUGGED))
	{
		app_context_reset_line_state();
	}
}

/**
 * @brief Supervise task progress and feed the hardware watchdog when healthy.
 *
 * Every 100 ms this task checks the fatal-error flag, task progress deadlines,
 * and that every application task has a non-suspended handle. It withholds
 * watchdog updates when any check fails, allowing the hardware to reset the
 * device.
 *
 * The CDC heartbeat is published by the CDC task and CDC data callbacks;
 * descriptor callbacks do not publish progress. The status LED task reports
 * progress after each error-pattern pulse.
 *
 * @param[in,out] params Pointer to the supervisor's task properties.
 *
 * @note Runs as a FreeRTOS task on core 0; it is not an ISR or TinyUSB callback.
 */
static void watchdog_task(void *params)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *task_prop = (task_props_t *)params;
	uint32_t loop_count = 0U;
	for (;;)
	{
		if (0U == (loop_count & 15U))
		{
			task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;

		bool healthy = !error_management_has_fatal_error() && task_health_check(time_us_32());
		for (uint32_t i = 0U; i < (uint32_t)NUM_TASKS; i++)
		{
			TaskHandle_t handle = app_context_task_props((task_enum_t)i)->task_handle;
			if ((handle == NULL) || (eTaskGetState(handle) == eSuspended))
			{
				healthy = false;
			}
		}
		if (healthy)
		{
			watchdog_update();
		}
		vTaskDelay(pdMS_TO_TICKS(100U));
	}
}

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
 * @param[in] failure_type  Error type to flag on allocation failure.
 * @return `true` when the task was created successfully.
 */
static inline bool create_task_with_affinity(TaskFunction_t function,
                                             const char *name,
                                             configSTACK_DEPTH_TYPE stack_size,
                                             void *params,
                                             UBaseType_t priority,
                                             task_enum_t task_id,
                                             UBaseType_t affinity_mask,
                                             error_type_t failure_type)
{
	task_props_t *const props = app_context_task_props(task_id);
	const BaseType_t result = xTaskCreate(function,
	                                      name,
	                                      stack_size,
	                                      params,
	                                      priority,
	                                      &props->task_handle);
	bool success = false;

	if (pdPASS == result)
	{
		vTaskCoreAffinitySet(props->task_handle, affinity_mask);
		success = true;
	}
	else
	{
		props->task_handle = NULL;

		if (error_management_is_fatal(failure_type))
		{
			fatal_halt(failure_type);
		}
		else
		{
			error_management_set_error_state(failure_type);
		}
	}

	return success;
}

/**
 * @brief Helper to create a queue and flag an error when allocation fails.
 *
 * @param[in] length    Queue length.
 * @param[in] item_size Size of each item.
 * @param[in] failure_type Error type to flag on allocation failure.
 * @return The queue handle or `NULL` when allocation fails.
 */
static inline QueueHandle_t create_queue_or_flag(UBaseType_t length, UBaseType_t item_size, error_type_t failure_type)
{
	QueueHandle_t queue = xQueueCreate(length, item_size);
	if (NULL == queue)
	{
		if (error_management_is_fatal(failure_type))
		{
			fatal_halt(failure_type);
		}
		else
		{
			error_management_set_error_state(failure_type);
		}
	}
	return queue;
}

/**
 * @brief Delete a task if it has a handle and clear its bookkeeping state.
 *
 * @param[in] task_id Identifier of the task to delete.
 *
 * @note Used during task-subsystem startup rollback and pre-scheduler cleanup.
 */
static void delete_task_if_exists(task_enum_t task_id)
{
	task_props_t *props = app_context_task_props(task_id);
	if (props->task_handle != NULL)
	{
		vTaskDelete(props->task_handle);
		props->task_handle = NULL;
	}
	props->high_watermark = 0U;
}

/**
 * @brief Create the I/O, outbound-event, status-LED, and watchdog tasks.
 *
 * The watchdog supervisor is created only after all application tasks succeed.
 *
 * @return `true` when all tasks were created; otherwise `false`.
 */
bool app_tasks_create_application(void)
{
	bool success;

	{
		success = create_task_with_affinity(led_status_task,
		                                    "led_status_task",
		                                    LED_STATUS_STACK_SIZE,
		                                    (void *)app_context_task_props(LED_STATUS_TASK),
		                                    mainLED_STATUS_TASK_PRIORITY,
		                                    LED_STATUS_TASK,
		                                    LED_STATUS_TASK_CORE_AFFINITY,
		                                    ERROR_RESOURCE_ALLOCATION);
	}

	if (success)
	{
		success = create_task_with_affinity(process_outbound_task,
		                                    "process_outbound_task",
		                                    PROCESS_OUTBOUND_STACK_SIZE,
		                                    (void *)app_context_task_props(PROCESS_OUTBOUND_TASK),
		                                    mainPROCESS_QUEUE_TASK_PRIORITY,
		                                    PROCESS_OUTBOUND_TASK,
		                                    PROCESS_OUTBOUND_TASK_CORE_AFFINITY,
		                                    ERROR_RESOURCE_ALLOCATION);
	}

	if (success)
	{
		success = create_task_with_affinity(adc_read_task,
		                                    "adc_read_task",
		                                    ADC_READ_STACK_SIZE,
		                                    (void *)app_context_task_props(ADC_READ_TASK),
		                                    mainADC_TASK_PRIORITY,
		                                    ADC_READ_TASK,
		                                    ADC_READ_TASK_CORE_AFFINITY,
		                                    ERROR_RESOURCE_ALLOCATION);
	}

	if (success)
	{
		success = create_task_with_affinity(keypad_task,
		                                    "keypad_task",
		                                    KEYPAD_STACK_SIZE,
		                                    (void *)app_context_task_props(KEYPAD_TASK),
		                                    mainKEY_TASK_PRIORITY,
		                                    KEYPAD_TASK,
		                                    KEYPAD_TASK_CORE_AFFINITY,
		                                    ERROR_RESOURCE_ALLOCATION);
	}

	if (success)
	{
		task_health_reset(time_us_32());
		task_props_t *const supervisor_props = app_context_supervisor_props();
		success = (xTaskCreate(watchdog_task, "watchdog", WATCHDOG_TASK_STACK_SIZE,
		                       supervisor_props, mainCDC_TASK_PRIORITY + 1U,
		                       &supervisor_props->task_handle) == pdPASS);
		if (success)
		{
			vTaskCoreAffinitySet(supervisor_props->task_handle, WATCHDOG_TASK_CORE_AFFINITY);
		}
	}
	return success;
}

/**
 * @brief Create the TinyUSB, CDC receive, decode, and transmit tasks and queues.
 *
 * Partial task or queue creation is rolled back before returning on failure.
 *
 * @return `true` when the communication subsystem was created; otherwise `false`.
 */
bool app_tasks_create_comm(void)
{
	bool success;
	QueueHandle_t encoded_queue = NULL;
	QueueHandle_t transmit_queue = NULL;

	{
		success = create_task_with_affinity(cdc_task,
		                                    "cdc_task",
		                                    CDC_STACK_SIZE,
		                                    (void *)app_context_task_props(CDC_TASK),
		                                    mainCDC_TASK_PRIORITY,
		                                    CDC_TASK,
		                                    CDC_TASK_CORE_AFFINITY,
		                                    ERROR_USB_INIT);
	}

	if (success)
	{
		encoded_queue = create_queue_or_flag(ENCODED_QUEUE_SIZE, sizeof(encoded_frame_t), ERROR_USB_INIT);
		app_context_set_encoded_queue(encoded_queue);
		if (NULL == encoded_queue)
		{
			success = false;
		}
	}

	if (success)
	{
		success = create_task_with_affinity(uart_event_task,
		                                    "uart_event_task",
		                                    UART_EVENT_STACK_SIZE,
		                                    (void *)app_context_task_props(UART_EVENT_TASK),
		                                    mainUART_TASK_PRIORITY,
		                                    UART_EVENT_TASK,
		                                    UART_EVENT_TASK_CORE_AFFINITY,
		                                    ERROR_USB_INIT);
	}

	if (success)
	{
		success = create_task_with_affinity(decode_reception_task,
		                                    "decode_reception_task",
		                                    DECODE_RECEPTION_STACK_SIZE,
		                                    (void *)app_context_task_props(DECODE_RECEPTION_TASK),
		                                    mainDECODE_TASK_PRIORITY,
		                                    DECODE_RECEPTION_TASK,
		                                    DECODE_RECEPTION_TASK_CORE_AFFINITY,
		                                    ERROR_USB_INIT);
	}

	if (success)
	{
		transmit_queue = create_queue_or_flag(CDC_TRANSMIT_QUEUE_SIZE, sizeof(cdc_packet_t), ERROR_USB_INIT);
		app_context_set_cdc_transmit_queue(transmit_queue);
		if (NULL == transmit_queue)
		{
			success = false;
		}
	}

	if (success)
	{
		success = create_task_with_affinity(cdc_write_task,
		                                    "cdc_write_task",
		                                    CDC_STACK_SIZE,
		                                    (void *)app_context_task_props(CDC_WRITE_TASK),
		                                    mainCDC_WRITE_TASK_PRIORITY,
		                                    CDC_WRITE_TASK,
		                                    CDC_TASK_CORE_AFFINITY,
		                                    ERROR_USB_INIT);
	}

	if (!success)
	{
		cleanup_comm_subsystem();
	}

	return success;
}

/**
 * @brief Delete application tasks and the data-event queue during boot cleanup.
 *
 * Cleanup is performed only before the scheduler starts, when no running task
 * can still use the queue handles.
 */
void app_tasks_cleanup_application(void)
{
	// Boot-only lifecycle: running tasks may hold the queue handles.
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
	{
		task_props_t *const supervisor_props = app_context_supervisor_props();
		if (supervisor_props->task_handle != NULL)
		{
			vTaskDelete(supervisor_props->task_handle);
		}
		supervisor_props->task_handle = NULL;
		supervisor_props->high_watermark = 0U;
		QueueHandle_t queue = app_context_get_data_event_queue();
		if (queue != NULL)
		{
			vQueueDelete(queue);
		}
		app_context_set_data_event_queue(NULL);

		delete_task_if_exists(PROCESS_OUTBOUND_TASK);
		delete_task_if_exists(ADC_READ_TASK);
		delete_task_if_exists(KEYPAD_TASK);
		delete_task_if_exists(LED_STATUS_TASK);
	}
}

/**
 * @brief Release communication queues and tasks after startup failure.
 *
 * @note Called during task creation, before the scheduler starts.
 */
static void cleanup_comm_subsystem(void)
{
	QueueHandle_t queue = app_context_get_encoded_queue();
	if (queue != NULL)
	{
		vQueueDelete(queue);
	}
	app_context_set_encoded_queue(NULL);

	queue = app_context_get_cdc_transmit_queue();
	if (queue != NULL)
	{
		vQueueDelete(queue);
	}
	app_context_set_cdc_transmit_queue(NULL);

	delete_task_if_exists(CDC_TASK);
	delete_task_if_exists(UART_EVENT_TASK);
	delete_task_if_exists(DECODE_RECEPTION_TASK);
	delete_task_if_exists(CDC_WRITE_TASK);
}

/**
 * @brief Task that reads raw CDC bytes, assembles complete COBS frames and
 *        enqueues them for decoding.
 *
 * The task is event-driven: the TinyUSB CDC RX callback
 * (@c tud_cdc_rx_cb in @ref app_comm.c) issues a task notification whenever
 * new bytes are pushed into the RX FIFO, waking this task.  Between
 * notifications the task blocks on @c ulTaskNotifyTake and does not burn
 * CPU.  A safety timeout of @ref UART_EVENT_TASK_WAIT_MS is used so that the
 * task still periodically services the FIFO and refreshes the watchdog even
 * if a notification is missed (for example during startup, before the host
 * is connected, or if the callback fires before the task handle is
 * registered in the application context).
 *
 * Each wake-up drains the FIFO in bulk chunks of @ref CDC_READ_CHUNK_SIZE
 * bytes, feeding bytes into an @ref encoded_framer_t state machine. Overflow
 * discards input through the next zero delimiter. Complete frames are tagged
 * with the current session before being pushed to the encoded queue.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void uart_event_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *task_prop = (task_props_t *)pvParameters;
	uint8_t receive_buffer[CDC_READ_CHUNK_SIZE];
	encoded_framer_t framer;
	encoded_frame_t frame;
	uint32_t loop_count = 0U;
	uint32_t session = app_context_cdc_session();

	encoded_framer_reset(&framer);

	for (;;)
	{
		/* Full-stack watermark scans are diagnostic-only; refresh every
		 * 64th pass to keep them off the RX hot path. */
		if (0U == (loop_count & 63U))
		{
			task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;
		task_health_progress(UART_EVENT_TASK, time_us_32());

		QueueHandle_t queue = app_context_get_encoded_queue();

		/* Drain the CDC RX FIFO until empty, yielding during sustained
		 * bursts so equal-priority work can run without relying on a
		 * pending RX notification to resume an existing backlog. */
		uint32_t consecutive_full = 0U;
		for (;;)
		{
			uint32_t current_session = app_context_cdc_session();
			if (session != current_session)
			{
				encoded_framer_reset(&framer);
				session = current_session;
			}
			const uint32_t count = tud_cdc_n_read(0, receive_buffer, sizeof(receive_buffer));
			if (0U == count)
			{
				break;
			}
			if (count == sizeof(receive_buffer))
			{
				consecutive_full++;
			}
			else
			{
				consecutive_full = 0U;
			}
			statistics_add_to_counter(BYTES_RECEIVED, count);
			if (!usb_ready() || (session != app_context_cdc_session()))
			{
				encoded_framer_reset(&framer);
				continue;
			}

			for (uint32_t i = 0U; i < count; i++)
			{
				const framer_result_t result = encoded_framer_push_byte(&framer, receive_buffer[i], &frame);
				switch (result)
				{
				case FRAMER_FRAME_READY:
					frame.session = session;
					if ((NULL == queue) || (xQueueSend(queue, &frame, pdMS_TO_TICKS(QUEUE_RETRY_DELAY_MS)) != pdTRUE))
					{
						statistics_increment_counter(QUEUE_SEND_ERROR);
					}
					break;
				case FRAMER_EMPTY_FRAME:
					statistics_increment_counter(COBS_DECODE_ERROR);
					break;
				case FRAMER_OVERFLOW:
					statistics_increment_counter(RECEIVE_BUFFER_OVERFLOW_ERROR);
					break;
				case FRAMER_NEED_MORE_DATA:
				default:
					break;
				}
			}
			if (consecutive_full >= CDC_RX_FULL_READ_YIELD_THRESHOLD)
			{
				consecutive_full = 0U;
				task_health_progress(UART_EVENT_TASK, time_us_32());
				taskYIELD();
			}
		}

		/* Block until tud_cdc_rx_cb notifies us or the safety timeout
		 * elapses.  Timeout keeps the task alive for watchdog /
		 * watermark updates even when the host is idle. */
		(void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(UART_EVENT_TASK_WAIT_MS));
	}
}

/**
 * @brief TinyUSB device task responsible for servicing the USB stack.
 *
 * With CFG_TUSB_OS=OPT_OS_FREERTOS, tud_task_ext() blocks on the TinyUSB
 * event queue until an IRQ posts work, so this task consumes no CPU while
 * the bus is idle.  A bounded timeout is used instead of WAIT_FOREVER so
 * the task still wakes periodically to pet the watchdog and refresh the
 * stack watermark when the link is quiet.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void cdc_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *task_prop = (task_props_t *)pvParameters;
	uint32_t loop_count = 0U;
	for (;;)
	{
		tud_task_ext(CDC_TASK_SAFETY_TIMEOUT_MS, false);
		uint8_t lines = tud_cdc_n_get_line_state(0);
		bool ready = tud_cdc_n_ready(0);
		app_context_set_line_state(ready && ((lines & 1U) != 0U), ready && ((lines & 2U) != 0U));
		if (0U == (loop_count & 63U))
		{
			task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;
		task_health_progress(CDC_TASK, time_us_32());
	}
}

/**
 * @brief Decode one received frame and dispatch it when it belongs to the current session.
 *
 * Frames from a previous USB session are dropped silently.
 *
 * @param[in]  frame         Encoded frame taken from the reception queue.
 * @param[out] decode_buffer Scratch buffer receiving the decoded packet.
 * @param[in]  capacity      Size of @p decode_buffer in bytes.
 */
static void decode_frame(const encoded_frame_t *frame, uint8_t *decode_buffer, size_t capacity)
{
	if (app_context_is_cdc_ready() && (frame->session == app_context_cdc_session()))
	{
		size_t num_decoded = 0U;

		if (0U == frame->length)
		{
			statistics_increment_counter(COBS_DECODE_ERROR);
		}
		else if (cobs_decode_checked(frame->data, frame->length, decode_buffer, capacity, &num_decoded) &&
		         (num_decoded > 0U))
		{
			app_comm_process_inbound(decode_buffer, num_decoded);
		}
		else
		{
			statistics_increment_counter(COBS_DECODE_ERROR);
		}
	}
}

/**
 * @brief Decode queued COBS frames and dispatch packets from the current session.
 *
 * Frames tagged with an earlier CDC session are silently dropped. The task
 * marks its indefinite queue wait as intentional so the watchdog supervisor
 * does not treat an idle receive queue as a stalled task.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void decode_reception_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *task_prop = (task_props_t *)pvParameters;
	encoded_frame_t frame;
	uint8_t decode_buffer[MAX_ENCODED_BUFFER_SIZE];
	uint32_t loop_count = 0U;

	for (;;)
	{
		if (0U == (loop_count & 63U))
		{
			task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;
		task_health_progress(DECODE_RECEPTION_TASK, time_us_32());

		// Get queue handle and wait if not created yet
		QueueHandle_t queue = app_context_get_encoded_queue();
		if (NULL == queue)
		{
			vTaskDelay(pdMS_TO_TICKS(QUEUE_RETRY_DELAY_MS));
			continue;
		}

		// An empty queue is an intentional indefinite wait.
		task_health_wait(DECODE_RECEPTION_TASK);
		if (pdFALSE == xQueueReceive(queue, &frame, portMAX_DELAY))
		{
			statistics_increment_counter(QUEUE_RECEIVE_ERROR);
		}
		else
		{
			task_health_progress(DECODE_RECEPTION_TASK, time_us_32());
			decode_frame(&frame, decode_buffer, sizeof(decode_buffer));
		}
	}
}

/**
 * @brief Schedule retained input snapshots and relative events for USB output.
 *
 * Absolute keypad and ADC values are selected round-robin and remain pending
 * until the writer completes the packet. Relative events are dequeued
 * best-effort, and each loop sends at most eight snapshots before yielding.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void process_outbound_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *props = pvParameters;
	uint32_t loop_count = 0U;
	for (;;)
	{
		task_health_progress(PROCESS_OUTBOUND_TASK, time_us_32());
		uint32_t session = app_context_cdc_session();
		input_delivery_session(session);
		if (app_context_is_cdc_ready())
		{
			data_events_t event;
			uint8_t slot;
			uint32_t version;
			// Bounded batch keeps input scanning schedulable.
			bool sending = true;
			for (uint32_t i = 0U; sending && (i < 8U); i++)
			{
				if (!input_delivery_next(&event, &slot, &version))
				{
					sending = false;
				}
				else if (!app_comm_send_input(&event, slot, version, session))
				{
					input_delivery_complete(slot, version, session, false);
					sending = false;
				}
				else
				{
					// Snapshot queued; completion is confirmed by the writer
				}
			}
			QueueHandle_t queue = app_context_get_data_event_queue();
			if ((queue != NULL) && (xQueueReceive(queue, &event, 0U) == pdPASS) &&
			    (event.session == session))
			{
				(void)app_comm_send_input(&event, INPUT_SNAPSHOT_NONE, 0U, session);
			}
		}
		if ((loop_count & 63U) == 0U)
		{
			props->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;
		vTaskDelay(pdMS_TO_TICKS(1U));
	}
}

/**
 * @brief Transmit queued encoded packets over USB CDC.
 *
 * A line-state session change clears any partial TinyUSB TX FIFO contents.
 * Packets from stale sessions are abandoned; an absolute snapshot is marked
 * delivered only after the full packet is accepted by the USB writer.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void cdc_write_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
	task_props_t *props = pvParameters;
	uint32_t session = app_context_cdc_session();
	uint32_t loop_count = 0U;
	for (;;)
	{
		task_health_progress(CDC_WRITE_TASK, time_us_32());
		uint32_t next_session = app_context_cdc_session();
		if (session != next_session)
		{
			// Clear stale FIFO fragments before writing the new session.
			(void)tud_cdc_n_write_clear(0);
			session = next_session;
		}
		QueueHandle_t queue = app_context_get_cdc_transmit_queue();
		cdc_packet_t packet;
		// Bounded wait also observes session changes when no packets arrive.
		if ((queue != NULL) && (xQueueReceive(queue, &packet, pdMS_TO_TICKS(10U)) == pdTRUE))
		{
			uint32_t active_session = app_context_cdc_session();
			if (session != active_session)
			{
				(void)tud_cdc_n_write_clear(0);
				session = active_session;
			}
			size_t written_total = 0U;
			uint32_t stalled_since = time_us_32();
			bool stall_reported = false;
			while (usb_ready() && (packet.session == app_context_cdc_session()) &&
			       (packet.session == session) && (written_total < packet.length))
			{
				uint32_t available = tud_cdc_n_write_available(0);
				uint32_t remaining = (uint32_t)packet.length - (uint32_t)written_total;
				uint32_t amount = (available < remaining) ? available : remaining;
				uint32_t written = 0U;
				if (amount != 0U)
				{
					written = tud_cdc_n_write(0, &packet.data[written_total], amount);
				}
				written_total += written;
				(void)tud_cdc_write_flush();
				if (written == 0U)
				{
					if (!stall_reported && ((uint32_t)(time_us_32() - stalled_since) >= 1000000U))
					{
						statistics_increment_counter(CDC_QUEUE_SEND_ERROR);
						stall_reported = true;
					}
					/* Keep this partial frame until progress or disconnect;
					 * dropping its tail would corrupt the following frame. */
					vTaskDelay(pdMS_TO_TICKS(10U));
				}
				else
				{
					stalled_since = time_us_32();
					stall_reported = false;
				}
				task_health_progress(CDC_WRITE_TASK, time_us_32());
			}
			bool complete = (written_total == packet.length) && usb_ready() &&
			                (packet.session == app_context_cdc_session());
			input_delivery_complete(packet.snapshot_slot, packet.snapshot_version, packet.session, complete);
			if (!complete)
			{
				statistics_increment_counter(CDC_QUEUE_SEND_ERROR);
			}
			statistics_add_to_counter(BYTES_SENT, (uint32_t)written_total);
		}
		if ((loop_count & 63U) == 0U)
		{
			props->high_watermark = uxTaskGetStackHighWaterMark(NULL);
		}
		loop_count++;
	}
}

/**
 * @brief Drive the status LED and publish progress heartbeats.
 *
 * In error state the task emits the configured blink pattern and reports
 * progress after each pulse so long patterns do not appear stalled. In normal
 * state it polls the USB connection LED at @ref LED_POLL_INTERVAL_MS.
 *
 * @param[in,out] pvParameters Pointer to the owning task properties structure.
 */
static void led_status_task(void *pvParameters)
{
	// cppcheck-suppress misra-c2012-11.5 ; Deviation D6: FreeRTOS passes task parameters and heap blocks as void *
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

				if ((i + 1U) < blink_count)
				{
					vTaskDelay(pdMS_TO_TICKS(BLINK_OFF_MS));
				}
				// A long blink pattern must not look like a hang to the supervisor.
				task_health_progress(LED_STATUS_TASK, time_us_32());
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

			vTaskDelay(pdMS_TO_TICKS(LED_POLL_INTERVAL_MS));
		}

		task_health_progress(LED_STATUS_TASK, time_us_32());
		task_prop->high_watermark = uxTaskGetStackHighWaterMark(NULL);
	}
}

/**
 * @brief FreeRTOS hook that retrieves the current runtime counter in microseconds.
 *
 * Backs portGET_RUN_TIME_COUNTER_VALUE() so the kernel can accumulate per-task
 * run time for the PC_TASK_STATUS_CMD reporting path (see app_comm.c).
 *
 * @return Monotonic runtime value used by the kernel statistics module.
 */
// MISRA Deviation D2 (Rule 8.7): hook referenced by lib/ code
uint32_t ulPortGetRunTime(void)
{
	return time_us_32();
}
