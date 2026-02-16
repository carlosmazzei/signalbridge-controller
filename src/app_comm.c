/**
 * @file app_comm.c
 * @brief Communication helpers for USB CDC data exchange.
 */

#include "app_comm.h"

#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "cobs.h"
#include "commands.h"
#include "error_management.h"
#include "app_outputs.h"

#include "app_config.h"
#include "app_context.h"

/**
 * @brief Callback invoked when line state changes (DTR, RTS).
 *
 * @param[in] itf Interface number.
 * @param[in] dtr Data Terminal Ready state.
 * @param[in] rts Request To Send state.
 */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
	(void)itf;
	app_context_set_line_state(dtr, rts);
}

/**
 * @brief Calculates the XOR checksum.
 *
 * @param[in] data   Pointer to the data.
 * @param[in] length Length of the data.
 * @return The calculated XOR checksum.
 */
static inline uint8_t calculate_checksum(const uint8_t *data, uint8_t length)
{
	uint8_t checksum = 0U;
	for (uint8_t i = 0U; i < length; i++)
	{
		checksum ^= data[i];
	}
	return checksum;
}

/**
 * @brief Send specific error counter status to the host.
 *
 * @param[in] index Index of the error counter to be sent.
 */
static inline void send_status(uint8_t index)
{
	uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};

	if (index < (uint8_t)NUM_STATISTICS_COUNTERS)
	{
		data[0] = index;
		const uint32_t counter_value = statistics_get_counter((statistics_counter_enum_t)index);
		data[1] = (uint8_t)((counter_value >> 24U) & 0xFFU);
		data[2] = (uint8_t)((counter_value >> 16U) & 0xFFU);
		data[3] = (uint8_t)((counter_value >> 8U) & 0xFFU);
		data[4] = (uint8_t)(counter_value & 0xFFU);
	}

	app_comm_send_packet(BOARD_ID, PC_ERROR_STATUS_CMD, data, sizeof(data));
}

/**
 * @brief Sends the heap usage (high watermark) of each task to the host.
 *
 * @param[in] index Index of the task whose information is requested.
 */
static void send_heap_status(uint8_t index)
{
	uint8_t data[13] = {0};
	bool done = false;

	if (index > (uint8_t)NUM_TASKS)
	{
		data[0] = INVALID_TASK_INDEX;
		app_comm_send_packet(BOARD_ID, PC_TASK_STATUS_CMD, data, 1U);
		done = true;
	}

	if (!done)
	{
		uint32_t value = 0U;
		if (index == (uint8_t)NUM_TASKS)
		{
			data[0] = index;

			value = ulTaskGetIdleRunTimeCounter();
			data[1] = (uint8_t)((value >> 24U) & 0xFFU);
			data[2] = (uint8_t)((value >> 16U) & 0xFFU);
			data[3] = (uint8_t)((value >> 8U) & 0xFFU);
			data[4] = (uint8_t)(value & 0xFFU);

			value = ulTaskGetIdleRunTimePercent();
			data[5] = (uint8_t)((value >> 24U) & 0xFFU);
			data[6] = (uint8_t)((value >> 16U) & 0xFFU);
			data[7] = (uint8_t)((value >> 8U) & 0xFFU);
			data[8] = (uint8_t)(value & 0xFFU);

			value = xPortGetMinimumEverFreeHeapSize();
			data[9] = (uint8_t)((value >> 24U) & 0xFFU);
			data[10] = (uint8_t)((value >> 16U) & 0xFFU);
			data[11] = (uint8_t)((value >> 8U) & 0xFFU);
			data[12] = (uint8_t)(value & 0xFFU);

			app_comm_send_packet(BOARD_ID, PC_TASK_STATUS_CMD, data, sizeof(data));
		}
		else
		{
			data[0] = index;

			const task_props_t *const props = app_context_task_props((task_enum_t)index);

			value = ulTaskGetRunTimeCounter(props->task_handle);
			data[1] = (uint8_t)((value >> 24U) & 0xFFU);
			data[2] = (uint8_t)((value >> 16U) & 0xFFU);
			data[3] = (uint8_t)((value >> 8U) & 0xFFU);
			data[4] = (uint8_t)(value & 0xFFU);

			value = ulTaskGetRunTimePercent(props->task_handle);
			data[5] = (uint8_t)((value >> 24U) & 0xFFU);
			data[6] = (uint8_t)((value >> 16U) & 0xFFU);
			data[7] = (uint8_t)((value >> 8U) & 0xFFU);
			data[8] = (uint8_t)(value & 0xFFU);

			value = props->high_watermark;
			data[9] = (uint8_t)((value >> 24U) & 0xFFU);
			data[10] = (uint8_t)((value >> 16U) & 0xFFU);
			data[11] = (uint8_t)((value >> 8U) & 0xFFU);
			data[12] = (uint8_t)(value & 0xFFU);

			app_comm_send_packet(BOARD_ID, PC_TASK_STATUS_CMD, data, sizeof(data));
		}
	}
}

void app_comm_send_packet(uint16_t id, uint8_t command, const uint8_t *send_data, uint8_t length)
{
	uint8_t uart_outbound_buffer[MESSAGE_SIZE];
	bool error = false;

	if (NULL == send_data)
	{
		statistics_increment_counter(QUEUE_SEND_ERROR);
		error = true;
	}

	if ((!error) && (length > DATA_BUFFER_SIZE))
	{
		statistics_increment_counter(BUFFER_OVERFLOW_ERROR);
		error = true;
	}

	if (!error)
	{
		uint16_t panel_id = id;
		panel_id <<= 5U;
		uart_outbound_buffer[0] = (uint8_t)(panel_id >> 8U);
		uart_outbound_buffer[1] = (uint8_t)((panel_id & 0xE0U) | (command & 0x1FU));
		uart_outbound_buffer[2] = length;

		(void)memcpy(&uart_outbound_buffer[HEADER_SIZE], send_data, length); // flawfinder: ignore

		const uint8_t checksum = calculate_checksum(uart_outbound_buffer, (uint8_t)(length + HEADER_SIZE));
		uart_outbound_buffer[HEADER_SIZE + length] = checksum;

		uint8_t encode_buffer[MAX_ENCODED_BUFFER_SIZE];
		const size_t num_encoded = cobs_encode(uart_outbound_buffer,
		                                       (size_t)length + HEADER_SIZE + CHECKSUM_SIZE,
		                                       encode_buffer);

		if ((num_encoded + 1U) > MAX_ENCODED_BUFFER_SIZE)
		{
			statistics_increment_counter(BUFFER_OVERFLOW_ERROR);
		}
		else
		{
			encode_buffer[num_encoded] = PACKET_MARKER;

			cdc_packet_t packet = {0};
			packet.length = (uint8_t)num_encoded + 1U;
			(void)memcpy(packet.data, encode_buffer, packet.length); // flawfinder: ignore

			QueueHandle_t queue = app_context_get_cdc_transmit_queue();
			if ((queue != NULL) && (pdTRUE == xQueueSend(queue, &packet, pdMS_TO_TICKS(1))))
			{
				// Enqueued successfully.
			}
			else
			{
				statistics_increment_counter(CDC_QUEUE_SEND_ERROR);
			}
		}
	}
}

void app_comm_process_inbound(const uint8_t *rx_buffer, size_t length)
{
	bool done = false;

	if (length < (HEADER_SIZE + CHECKSUM_SIZE))
	{
		statistics_increment_counter(MSG_MALFORMED_ERROR);
		done = true;
	}

	uint16_t rxID = 0U;
	uint8_t cmd = 0U;
	uint8_t len = 0U;
	if (!done)
	{
		rxID = (uint16_t)(rx_buffer[0] << 3U);
		rxID |= (uint16_t)((rx_buffer[1] & 0xE0U) >> 5U);
		cmd = (uint8_t)(rx_buffer[1] & 0x1FU);
		len = rx_buffer[2];

		if (length != ((size_t)len + HEADER_SIZE + CHECKSUM_SIZE))
		{
			statistics_increment_counter(MSG_MALFORMED_ERROR);
			done = true;
		}
	}

	if ((!done) && (DATA_BUFFER_SIZE < len))
	{
		statistics_increment_counter(BUFFER_OVERFLOW_ERROR);
		done = true;
	}

	if ((!done) && (rxID != BOARD_ID))
	{
		statistics_increment_counter(UNKNOWN_CMD_ERROR);
		done = true;
	}

	uint8_t decoded_data[DATA_BUFFER_SIZE] = {0};
	if (!done)
	{
		(void)memcpy(decoded_data, &rx_buffer[HEADER_SIZE], len); // flawfinder: ignore
		const uint8_t calculated_checksum = calculate_checksum(rx_buffer, (uint8_t)(len + HEADER_SIZE));
		const uint8_t received_checksum = rx_buffer[len + HEADER_SIZE];

		if (calculated_checksum != received_checksum)
		{
			statistics_increment_counter(CHECKSUM_ERROR);
			done = true;
		}
	}

	if (!done)
	{
		switch (cmd)
		{
		case PC_LEDOUT_CMD:
			if (led_out(decoded_data, len) != OUTPUT_OK)
			{
				statistics_increment_counter(LED_OUT_ERROR);
			}
			break;

		case PC_PWM_CMD:
			set_pwm_duty(decoded_data[0]);
			break;

		case PC_DPYCTL_CMD:
		{
			const output_result_t result = display_out(decoded_data, len);
			if (result != OUTPUT_OK)
			{
				statistics_increment_counter(DISPLAY_OUT_ERROR);
			}
		}
		break;

		case PC_ECHO_CMD:
			app_comm_send_packet(rxID, cmd, decoded_data, len);
			break;

		case PC_ERROR_STATUS_CMD:
			send_status(decoded_data[0]);
			break;

		case PC_TASK_STATUS_CMD:
			send_heap_status(decoded_data[0]);
			break;

		default:
			statistics_increment_counter(UNKNOWN_CMD_ERROR);
			break;
		}
	}
}
