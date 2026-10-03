/**
 * @file test_app_comm.c
 * @brief Unit tests for app_comm module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "app_comm.h"
#include "tusb.h"
#include "app_config.h"
#include "app_context.h"
#include "commands.h"
#include "error_management.h"

static QueueHandle_t mock_queue_handle = (QueueHandle_t)0xCAFEU;
static BaseType_t mock_queue_result = pdTRUE;
static int mock_queue_send_calls = 0;
static cdc_packet_t captured_packet;

BaseType_t __wrap_xQueueGenericSend(QueueHandle_t xQueue,
                                    const void *pvItemToQueue,
                                    TickType_t xTicksToWait,
                                    BaseType_t xCopyPosition)
{
	(void)xTicksToWait;
	(void)xCopyPosition;

	assert_ptr_equal(xQueue, mock_queue_handle);
	mock_queue_send_calls++;

	if (pvItemToQueue != NULL)
	{
		memcpy(&captured_packet, pvItemToQueue, sizeof(cdc_packet_t));
	}

	return mock_queue_result;
}

static int setup_test(void **state)
{
	(void)state;
	mock_queue_send_calls = 0;
	memset(&captured_packet, 0, sizeof(captured_packet));
	mock_queue_result = pdTRUE;
	statistics_reset_all_counters();
	app_context_set_cdc_transmit_queue(mock_queue_handle);
	return 0;
}

static void test_send_packet_accepts_max_payload(void **state)
{
	(void)state;
	uint8_t payload[DATA_BUFFER_SIZE];
	for (uint8_t i = 0U; i < DATA_BUFFER_SIZE; i++)
	{
		payload[i] = (uint8_t)(i + 1U);
	}

	app_comm_send_packet(BOARD_ID, PC_ECHO_CMD, payload, DATA_BUFFER_SIZE);

	assert_int_equal(mock_queue_send_calls, 1);
	assert_int_equal(statistics_get_counter(BUFFER_OVERFLOW_ERROR), 0);
	assert_true(captured_packet.length > 0U);
	assert_true(captured_packet.length <= MAX_ENCODED_BUFFER_SIZE);
	assert_int_equal(captured_packet.length, MAX_ENCODED_BUFFER_SIZE);
}

static void test_send_packet_rejects_oversized_payload(void **state)
{
	(void)state;
	uint8_t payload[DATA_BUFFER_SIZE + 1U];
	memset(payload, 0xA5, sizeof(payload));

	app_comm_send_packet(BOARD_ID, PC_ECHO_CMD, payload, (uint8_t)(DATA_BUFFER_SIZE + 1U));

	assert_int_equal(mock_queue_send_calls, 0);
	assert_int_equal(statistics_get_counter(BUFFER_OVERFLOW_ERROR), 1);
}

static void test_cdc_queue_memory_budget(void **state)
{
	(void)state;

	/* The CDC transmit queue is copy-by-value (one cdc_packet_t per slot),
	 * so its footprint comes straight out of the FreeRTOS heap. Keep the
	 * budget at or below 16 KB (12.5% of the 128 KB heap) while retaining
	 * enough depth to buffer well beyond the 4 KB TinyUSB TX FIFO. */
	assert_true((CDC_TRANSMIT_QUEUE_SIZE * sizeof(cdc_packet_t)) <= (16U * 1024U));
	assert_true(CDC_TRANSMIT_QUEUE_SIZE >= 128U);
}

static void test_reject_wrong_fixed_payload_length(void **state)
{
	(void)state;
	const uint8_t commands[] = {PC_PWM_CMD, PC_ERROR_STATUS_CMD, PC_TASK_STATUS_CMD};
	for (size_t i = 0U; i < sizeof(commands); i++)
	{
		for (uint8_t length = 0U; length <= 2U; length += 2U)
		{
			statistics_reset_all_counters();
			uint8_t packet[] = {0U, (uint8_t)(0x20U | commands[i]), length, 0U, 0U, 0U};
			packet[3U + length] = packet[1] ^ length;
			app_comm_process_inbound(packet, 4U + length);
			assert_int_equal(1U, statistics_get_counter(MSG_MALFORMED_ERROR));
		}
	}
}

static unsigned rx_flushes;
void tud_cdc_n_read_flush(uint8_t interface)
{
	assert_int_equal(0U, interface);
	rx_flushes++;
}

static void test_line_transition_discards_buffered_rx(void **state)
{
	(void)state;
	app_context_reset_line_state();
	rx_flushes = 0U;
	tud_cdc_line_state_cb(0U, true, true);
	assert_int_equal(1U, rx_flushes);
	tud_cdc_line_state_cb(0U, true, true);
	assert_int_equal(1U, rx_flushes);
	tud_cdc_line_state_cb(1U, false, false);
	assert_true(app_context_is_cdc_ready());
	assert_int_equal(1U, rx_flushes);
	tud_cdc_line_state_cb(0U, false, false);
	assert_false(app_context_is_cdc_ready());
	assert_int_equal(2U, rx_flushes);
}

static void test_usb_sessions(void **state)
{
	(void)state;
	app_context_reset_line_state();
	uint32_t before = app_context_cdc_session();
	app_context_set_line_state(true, false);
	assert_false(app_context_is_cdc_ready());
	assert_int_equal(before, app_context_cdc_session());
	app_context_set_line_state(true, true);
	uint32_t connected = app_context_cdc_session();
	assert_true(app_context_is_cdc_ready());
	assert_true(before != connected);
	app_context_set_line_state(true, true);
	assert_int_equal(connected, app_context_cdc_session());
	app_context_reset_line_state();
	assert_false(app_context_is_cdc_ready());
	assert_true(connected != app_context_cdc_session());
}

static void test_input_packet_tracks_session_and_failure(void **state)
{
	(void)state;
	data_events_t event = {.command = PC_KEY_CMD, .data_length = 1U, .data = {1U}};
	mock_queue_result = pdFALSE;
	assert_false(app_comm_send_input(&event, 7U, 42U, 3U));
	assert_int_equal(7U, captured_packet.snapshot_slot);
	assert_int_equal(42U, captured_packet.snapshot_version);
	assert_int_equal(3U, captured_packet.session);
	mock_queue_result = pdTRUE;
	assert_true(app_comm_send_input(&event, 7U, 42U, 3U));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_reject_wrong_fixed_payload_length),
		cmocka_unit_test(test_usb_sessions),
		cmocka_unit_test(test_line_transition_discards_buffered_rx),
		cmocka_unit_test_setup(test_input_packet_tracks_session_and_failure, setup_test),
		cmocka_unit_test_setup(test_send_packet_accepts_max_payload, setup_test),
		cmocka_unit_test_setup(test_send_packet_rejects_oversized_payload, setup_test),
		cmocka_unit_test_setup(test_cdc_queue_memory_budget, setup_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
