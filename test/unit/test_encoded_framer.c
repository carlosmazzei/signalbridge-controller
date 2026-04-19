/**
 * @file test_encoded_framer.c
 * @brief Unit tests for the encoded_framer byte->frame assembler.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "app_config.h"
#include "encoded_framer.h"

static int setup_framer(void **state)
{
	encoded_framer_t *framer = test_malloc(sizeof(encoded_framer_t));
	assert_non_null(framer);
	encoded_framer_reset(framer);
	*state = framer;
	return 0;
}

static int teardown_framer(void **state)
{
	test_free(*state);
	return 0;
}

static void push_bytes(encoded_framer_t *framer,
                       const uint8_t *bytes,
                       size_t count,
                       encoded_frame_t *out_frame,
                       framer_result_t *last_result)
{
	for (size_t i = 0U; i < count; i++)
	{
		*last_result = encoded_framer_push_byte(framer, bytes[i], out_frame);
	}
}

static void test_reset_clears_accumulator(void **state)
{
	encoded_framer_t *framer = *state;
	framer->length = 5U;
	encoded_framer_reset(framer);
	assert_int_equal(framer->length, 0U);
}

static void test_non_marker_byte_accumulates(void **state)
{
	encoded_framer_t *framer = *state;
	encoded_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	framer_result_t result = encoded_framer_push_byte(framer, 0x42U, &frame);

	assert_int_equal(result, FRAMER_NEED_MORE_DATA);
	assert_int_equal(framer->length, 1U);
	assert_int_equal(framer->buffer[0], 0x42U);
}

static void test_marker_after_bytes_yields_frame(void **state)
{
	encoded_framer_t *framer = *state;
	encoded_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	framer_result_t result = FRAMER_NEED_MORE_DATA;

	const uint8_t payload[] = {0x01U, 0x02U, 0x03U, 0x04U, 0x00U};
	push_bytes(framer, payload, sizeof(payload), &frame, &result);

	assert_int_equal(result, FRAMER_FRAME_READY);
	assert_int_equal(frame.length, 4U);
	assert_memory_equal(frame.data, payload, 4U);
	// framer must reset after emitting a frame
	assert_int_equal(framer->length, 0U);
}

static void test_marker_without_payload_is_empty_frame(void **state)
{
	encoded_framer_t *framer = *state;
	encoded_frame_t frame;
	memset(&frame, 0xFFU, sizeof(frame));

	framer_result_t result = encoded_framer_push_byte(framer, 0x00U, &frame);

	assert_int_equal(result, FRAMER_EMPTY_FRAME);
	assert_int_equal(framer->length, 0U);
}

static void test_two_consecutive_frames(void **state)
{
	encoded_framer_t *framer = *state;
	encoded_frame_t frame;
	framer_result_t result = FRAMER_NEED_MORE_DATA;

	const uint8_t stream[] = {
		0x11U, 0x22U, 0x00U,                // frame 1
		0xAAU, 0xBBU, 0xCCU, 0x00U          // frame 2
	};

	memset(&frame, 0, sizeof(frame));
	for (size_t i = 0U; i < 3U; i++)
	{
		result = encoded_framer_push_byte(framer, stream[i], &frame);
	}
	assert_int_equal(result, FRAMER_FRAME_READY);
	assert_int_equal(frame.length, 2U);
	assert_int_equal(frame.data[0], 0x11U);
	assert_int_equal(frame.data[1], 0x22U);

	memset(&frame, 0, sizeof(frame));
	for (size_t i = 3U; i < sizeof(stream); i++)
	{
		result = encoded_framer_push_byte(framer, stream[i], &frame);
	}
	assert_int_equal(result, FRAMER_FRAME_READY);
	assert_int_equal(frame.length, 3U);
	assert_int_equal(frame.data[0], 0xAAU);
	assert_int_equal(frame.data[1], 0xBBU);
	assert_int_equal(frame.data[2], 0xCCU);
}

static void test_overflow_resets_framer(void **state)
{
	encoded_framer_t *framer = *state;
	encoded_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	framer_result_t result = FRAMER_NEED_MORE_DATA;

	/* Push MAX_ENCODED_BUFFER_SIZE non-marker bytes; the last one must
	 * trigger overflow because there is no room left and no marker. */
	for (size_t i = 0U; i < MAX_ENCODED_BUFFER_SIZE; i++)
	{
		result = encoded_framer_push_byte(framer, 0xA5U, &frame);
	}

	assert_int_equal(result, FRAMER_OVERFLOW);
	assert_int_equal(framer->length, 0U);
}

static void test_frame_ready_works_with_null_out_frame(void **state)
{
	encoded_framer_t *framer = *state;

	(void)encoded_framer_push_byte(framer, 0x77U, NULL);
	framer_result_t result = encoded_framer_push_byte(framer, 0x00U, NULL);

	assert_int_equal(result, FRAMER_FRAME_READY);
	assert_int_equal(framer->length, 0U);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_reset_clears_accumulator, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_non_marker_byte_accumulates, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_marker_after_bytes_yields_frame, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_marker_without_payload_is_empty_frame, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_two_consecutive_frames, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_overflow_resets_framer, setup_framer, teardown_framer),
		cmocka_unit_test_setup_teardown(test_frame_ready_works_with_null_out_frame, setup_framer, teardown_framer),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
