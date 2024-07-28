/**
 * @file main.c
 * @brief Main file of the project
 * @author Carlos Mazzei
 *
 * This file contains the main function of the project.
 *
 * @copyright (c) 2020-2024 Carlos Mazzei
 * All rights reserved.
 */

#include "main.h"

/**
 * Queue to store cobs encoded received data
 */
static QueueHandle_t encoded_reception_queue = NULL;

/**
 * Queue to store data events to be sent to the host
 */
static QueueHandle_t data_event_queue = NULL;

/**
 * Store error counters
 */
static error_counters_t error_counters;

/**
 * Store task properties
 */
static task_props_t task_props[NUM_TASKS];

/** @brief Receive data from uart
 *
 *  Receives data from UART and post it to a reception queue. If the data is encoded, it should be decoded accordingly
 *
 *	@param pvParameters Pointer to parameters passed to the task
 */
static void uart_event_task(void *pvParameters)
{
	uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];
	task_props_t * task_prop = (task_props_t*) pvParameters;

	while (true)
	{
		/* Get free heap for the task */
		task_prop->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		/* Update watchdog timer */
		watchdog_update();

		if (!tud_cdc_n_available(0)) continue;

		uint32_t count = tud_cdc_n_read(0, receive_buffer, sizeof(receive_buffer));
		for (uint32_t i = 0; (i < count) && (i < MAX_ENCODED_BUFFER_SIZE); i++)
		{
			BaseType_t success = xQueueSend(encoded_reception_queue, &receive_buffer[i], portMAX_DELAY);
			if (success != pdTRUE)
				error_counters.counters[QUEUE_SEND_ERROR]++;
		}
	}
}

/** @brief CDC tasks
 *
 * 	CDC task needed to update the USB stack
 *
 * 	@param pvParameters Pointer to parameters passed to the task
 */
static void cdc_task(void *pvParameters)
{
	task_props_t * task_prop = (task_props_t*) pvParameters;

	while (true)
	{
		tud_task(); // tinyusb device task

		if (tud_cdc_n_connected(0))
			gpio_put(PICO_DEFAULT_LED_PIN, 1);
		else
			gpio_put(PICO_DEFAULT_LED_PIN, 0);

		/* Get free heap for the task */
		task_prop->high_watermark = (uint8_t)xPortGetFreeHeapSize();
		/* Update watchdog timer */
		watchdog_update();
	}
}

/** @brief Decode reception task
 *
 * Process bytes received from UART and decoded the data. The received data is using COBS encoding.
 *
 * @param pvParameters Pointer to parameters passed to the task
 */
static void decode_reception_task(void *pvParameters)
{
	uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];
	size_t receive_buffer_index = 0;
	task_props_t * task_prop = (task_props_t*) pvParameters;

	while (true)
	{
		/* Get free heap for the task */
		task_prop->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		/* Update watchdog timer */
		watchdog_update();

		/* Check if there is a problem dequeueing the data*/
		uint8_t data;
		if (xQueueReceive(encoded_reception_queue, (void *)&data, portMAX_DELAY) == pdFALSE)
		{
			error_counters.counters[QUEUE_RECEIVE_ERROR]++;
			continue;
		}

		if (data == PACKET_MARKER)
		{
			if (receive_buffer_index <= 0)
			{
				error_counters.counters[MSG_MALFORMED_ERROR]++;
				continue;
			}

			uint8_t decode_buffer[receive_buffer_index];
			size_t num_decoded = cobs_decode(receive_buffer, receive_buffer_index, decode_buffer);

			receive_buffer_index = 0;

			if (num_decoded > 0) process_inbound_data(decode_buffer, num_decoded);
		}
		else
		{
			if ((receive_buffer_index + 1) < MAX_ENCODED_BUFFER_SIZE)
				receive_buffer[receive_buffer_index++] = data;
			else
				error_counters.counters[RECEIVE_BUFFER_OVERFLOW_ERROR]++;
		}
	}
}

/** @brief Send error counters to host
 *
 *  Send error counters to the host and format the message in chunks of bytes
 *
 */
static inline void send_status(uint8_t index)
{
	uint8_t data[2];

	if (index < NUM_ERROR_COUNTERS)
	{
		data[0] = error_counters.counters[index] >> 8;
		data[1] = (uint8_t)error_counters.counters[index];
	}

	send_data(PANEL_ID, PC_STATUS_CMD, data, sizeof(data));
}

/** @brief Send status of the heap of each task
 *
 * Send heap usage of each task and overall heap usage from the idle task
 *
 */
void send_heap_status()
{
	uint8_t data[7];

	for (uint8_t t = 0; t < NUM_TASKS; t++)
	{
		data[t] = task_props[t].high_watermark;
	}

	send_data(PANEL_ID, PC_HEAP_STATUS_CMD, data, sizeof(data));
}

/** @brief Send a packet of data.
 *
 * This function will encode and send a packet of data. After
 * sending, it will send the specified `Packet Marker` defined.
 *
 * @param id The ID of the device sending the data.
 * @param command The command to send.
 * @param send_data A pointer to the data to send.
 * @param length The length of the data to send.
 */
static void send_data(uint16_t id, uint8_t command, const uint8_t *send_data, uint8_t length)
{
	uint8_t i;
	uint8_t uart_outbound_buffer[DATA_BUFFER_SIZE];

	id <<= 5;
	uart_outbound_buffer[0] = (id >> 8);
	uart_outbound_buffer[1] = id & 0xE0;
	uart_outbound_buffer[1] |= (command & 0x1F);
	uart_outbound_buffer[2] = length;

	for (i = 3; (i < uart_outbound_buffer[2] + 3) && (i < DATA_BUFFER_SIZE); i++)
	{
		uart_outbound_buffer[i] = *send_data;
		send_data++;
	}

	uint8_t encode_buffer[MAX_ENCODED_BUFFER_SIZE];
	size_t num_encoded = cobs_encode(uart_outbound_buffer, length + 3, encode_buffer);
	encode_buffer[num_encoded] = 0x00; // Ensure the last byte is 0

	for (i = 0; i < num_encoded + 1; i++)
	{
		tud_cdc_n_write_char(0, encode_buffer[i]);
	}
	tud_cdc_write_flush();
}

/** @brief Task to process inbound messages
 *
 * This function processes inbound messages from the reception queue
 *
 * @param rx_buffer A pointer to a data buffer to be processed
 * @param length Length of data to be processed
 */
static void process_inbound_data(const uint8_t *rx_buffer, size_t length)
{
	uint16_t rxID = 0;
	uint8_t len = 0;
	uint8_t decoded_data[DATA_BUFFER_SIZE];
	uint8_t leds[2];

	/* Check if inbound data is at least 3 bytes length. Otherwise return to not access invalid memory */
	if (length < 3)
	{
		/* Message has no header with id and command, so it cannot be processed */
		error_counters.counters[MSG_MALFORMED_ERROR]++;
		return;
	}

	/* Initialize decoded_data variable */
	memset(decoded_data, 0, DATA_BUFFER_SIZE);

	/* Decode id and length of the message */
	rxID = (uint16_t)(rx_buffer[0] << 3);
	rxID |= ((rx_buffer[1] & 0xE0) >> 5);
	decoded_data[0] = rx_buffer[1] & 0x1F;
	len = rx_buffer[2];

	/** @todo Check if there is a more efficient way to do it without needing to loop */
	for (uint8_t i = 1; (i < len + 1) && (i < DATA_BUFFER_SIZE - 2); i++)
	{
		decoded_data[i] = rx_buffer[i + 2];
	}

	switch (decoded_data[0])
	{
	/**
	 * Implement other inbound commands (LED, DISPLAY, etc)
	 */
	case PC_LEDOUT_CMD:
		leds[0] = decoded_data[2]; // Offset of the byte state to change
		leds[1] = decoded_data[3]; // States of the LEDs to change
		if (led_out(decoded_data[1], leds, sizeof(leds)) != true)
			error_counters.counters[LED_OUT_ERROR]++;
		break;

	case PC_PWM_CMD:
		set_pwm_duty(decoded_data[1]);
		break;

	case PC_DPYCTL_CMD:
		if (display_out(&decoded_data[1], len) != len)
			error_counters.counters[DISPLAY_OUT_ERROR]++;
		break;

	case PC_ECHO_CMD:
		send_data(rxID, decoded_data[0], &decoded_data[1], len);
		break;

	case PC_STATUS_CMD:
		send_status(decoded_data[1]);
		break;

	case PC_HEAP_STATUS_CMD:
		/* x00 x38 x00*/
		send_heap_status();
		break;

	default:
		break;
	}
}

/** @brief Task to send outbound messages
 *
 * This task reads hardware inputs and send data to the host
 *
 * @param pvParameters Arguments passed to the task
 */
static void process_outbound_task(void *pvParameters)
{
	task_props_t * task_prop = (task_props_t*) pvParameters;

	while (true)
	{
		data_events_t data_event;
		if (xQueueReceive(data_event_queue, (void *)&data_event, portMAX_DELAY))
		{
			send_data(PANEL_ID, data_event.command, data_event.data, data_event.data_length);
		}
		else
		{
			error_counters.counters[QUEUE_RECEIVE_ERROR]++;
		}
		/* Get free heap size */
		task_prop->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		/* Update watchdog timer */
		watchdog_update();
	}
}

/** @brief Setup hardware
 *
 * Setup hardware such as UART, LED, Keyboard and internal structures.
 *
 * @return Return true if initialization was successfull and false if failed to create any resource
 */
static inline bool setup_hardware(void)
{
	/* Initialize all IOs */
	stdio_init_all();

	/* Initialize outputs */
	output_init();

	/* Create queue to receive data events */
	data_event_queue = xQueueCreate(DATA_EVENT_QUEUE_SIZE, sizeof(data_events_t)); // The size of a single byte, created before hardware setup?
	if (data_event_queue == NULL)
		return false;

	/* Enable inputs (Keypad, ADC and Rotaries) */
	const input_config_t config = {
		.columns = 8,
		.rows = 8,
		.key_settling_time_ms = 20,
		.input_event_queue = data_event_queue,
		.adc_banks = 2,
		.adc_channels = 8,
		.adc_settling_time_ms = 100,
		.encoder_settling_time_ms = 10,
		.encoder_mask[7] = true
	};                        /* Enable encoder on row 8 (last row) */

	/* Initialize inputs */
	input_init(&config);

	/* Init error counters and handles */
	for (uint8_t i = 0; i < NUM_ERROR_COUNTERS; i++)
	{
		error_counters.counters[i] = 0;
	}

	/* Init task props */
	for (uint8_t i = 0; i < NUM_TASKS; i++)
	{
		task_props[i].high_watermark = 0;
		task_props[i].task_handle = NULL;
	}

	watchdog_enable(1000, true);

	return true;
}

/** @brief Enter error state
 *
 *  Enter error loop blinking led if any error ocurred during startup
 *
 */
static inline void enter_error_state()
{
	bool state = true;
	while (true)
	{
		state = !state;
		gpio_put(PICO_DEFAULT_LED_PIN, state);
		vTaskDelay(pdMS_TO_TICKS(500));
		watchdog_update();
	}
}

/** @brief Clean up all resources
 *
 * Clean up all the previously created resources in case of problems
 *
 */
static inline void clean_up()
{
	/* Delete all queues */
	if (encoded_reception_queue != NULL)
	{
		vQueueDelete(encoded_reception_queue);
		encoded_reception_queue = NULL;
	}

	if (data_event_queue != NULL)
	{
		vQueueDelete(data_event_queue);
		data_event_queue = NULL;
	}

	/* Delete all task handles previously created */
	for (uint8_t t = 0; t < NUM_TASKS; t++)
	{
		if (task_props[t].task_handle != NULL)
		{
			vTaskDelete(task_props[t].task_handle);
			task_props[t].task_handle = NULL;
		}
	}
}

/** @brief Main function
 *
 * Start the queues and task to handle input and output events
 *
 */
int main(void)
{
	/* Set error state to false */
	BaseType_t success;
	error_counters.error_state = false;

	/* Add error counter and cleanup resources */
	if (watchdog_caused_reboot())
	{
		error_counters.counters[WATCHDOG_ERROR]++;
		clean_up();
	}

	/* TinyUSB init */
	board_init();

	/* init device stack on configured roothub port */
	if (!tud_init(BOARD_TUD_RHPORT))
		error_counters.error_state = true;

	/* Initialize hardware specific config */
	if (!setup_hardware())
		error_counters.error_state = true;

	/* Create a task to handle UART event from ISR */
	success = xTaskCreate(cdc_task,
	                      "cdc_task",
	                      CDC_STACK_SIZE,
	                      (void *)&task_props[CDC_TASK],
	                      mainPROCESS_QUEUE_TASK_PRIORITY,
	                      &task_props[CDC_TASK].task_handle);
	if (success != pdPASS)
		error_counters.error_state = true;

	/* Create queue to receive encoded data */
	encoded_reception_queue = xQueueCreate(ENCODED_QUEUE_SIZE, sizeof(uint8_t)); // The size of a single byte
	if (encoded_reception_queue != NULL)
	{
		/* Created the queue  successfully, then create the UART event task */
		success = xTaskCreate(uart_event_task,
		                      "uart_event_task",
		                      UART_EVENT_STACK_SIZE,
		                      (void *)&task_props[UART_EVENT_TASK],
		                      mainPROCESS_QUEUE_TASK_PRIORITY,
		                      &task_props[UART_EVENT_TASK].task_handle);
		if (success != pdPASS)
			error_counters.error_state = true;

		success = xTaskCreate(decode_reception_task,
		                      "decode_reception_task",
		                      DECODE_RECEPTION_STACK_SIZE,
		                      (void *)&task_props[DECODE_RECEPTION_TASK],
		                      mainPROCESS_QUEUE_TASK_PRIORITY + 1,
		                      &task_props[DECODE_RECEPTION_TASK].task_handle);
		if (success != pdPASS)
			error_counters.error_state = true;
	}
	else
	{
		error_counters.error_state = true;
	}

	/* Create queue to receive data events */
	success = xTaskCreate(process_outbound_task,
	                      "process_outbound_task",
	                      PROCESS_OUTBOUND_STACK_SIZE,
	                      (void *)&task_props[PROCESS_OUTBOUND_TASK],
	                      mainPROCESS_QUEUE_TASK_PRIORITY,
	                      &task_props[PROCESS_OUTBOUND_TASK].task_handle);
	if (success != pdPASS)
		error_counters.error_state = true;

	/* Initiate task to read the inputs */
	success = xTaskCreate(adc_read_task, "adc_read_task",
	                      ADC_READ_STACK_SIZE,
	                      (void *)&task_props[ADC_READ_TASK],
	                      mainPROCESS_QUEUE_TASK_PRIORITY,
	                      &task_props[ADC_READ_TASK].task_handle);
	if (success != pdPASS)
		error_counters.error_state = true;

	success = xTaskCreate(keypad_task,
	                      "keypad_task",
	                      KEYPAD_STACK_SIZE,
	                      (void *)&task_props[KEYPAD_TASK],
	                      mainPROCESS_QUEUE_TASK_PRIORITY,
	                      &task_props[KEYPAD_TASK].task_handle);
	if (success != pdPASS)
		error_counters.error_state = true;

	success = xTaskCreate(encoder_read_task,
	                      "encoder_task",
	                      ENCODER_READ_STACK_SIZE,
	                      (void *)&task_props[ENCODER_READ_TASK],
	                      mainPROCESS_QUEUE_TASK_PRIORITY,
	                      &task_props[ENCODER_READ_TASK].task_handle);
	if (success != pdPASS)
		error_counters.error_state = true;

	/* Start the tasks and timer running. */
	if (error_counters.error_state == false)
		vTaskStartScheduler();
	else
		enter_error_state();

	/* Should not enter if everything initiated correctly */
	while (true)
	{
		/** @todo Try to recover from error state */
	}

	return 0;
}
