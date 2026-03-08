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

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_send_packet_accepts_max_payload, setup_test),
		cmocka_unit_test_setup(test_send_packet_rejects_oversized_payload, setup_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
