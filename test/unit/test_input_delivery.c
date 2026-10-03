#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "input_delivery.h"

static void delivery_and_recovery(void **state)
{
	(void)state;
	data_events_t event;
	uint8_t slot;
	uint32_t version;
	input_delivery_reset();
	input_delivery_session(1U);
	input_delivery_adc(3U, 2048U);
	assert_true(input_delivery_next(&event, &slot, &version));
	assert_int_equal(event.data[1], 8U);
	assert_false(input_delivery_next(&event, &slot, &version));
	input_delivery_complete(slot, version, 1U, false);
	assert_true(input_delivery_next(&event, &slot, &version));
	input_delivery_complete(slot, version, 1U, true);
	assert_false(input_delivery_next(&event, &slot, &version));
	input_delivery_session(2U);
	assert_true(input_delivery_next(&event, &slot, &version));
	input_delivery_complete(slot, version, 1U, true); // old session cannot ACK
	input_delivery_complete(slot, version, 2U, false);
	assert_true(input_delivery_next(&event, &slot, &version));
}

static void changes_during_transmission(void **state)
{
	(void)state;
	data_events_t event;
	uint8_t slot;
	uint32_t version;
	input_delivery_reset();
	input_delivery_key(0U, 1U, true);
	assert_true(input_delivery_next(&event, &slot, &version));
	input_delivery_key(0U, 1U, false);
	input_delivery_complete(slot, version, 0U, true);
	assert_true(input_delivery_next(&event, &slot, &version));
	assert_int_equal(event.data[0], 0x10U);
	input_delivery_complete(slot, version, 0U, true);
	assert_false(input_delivery_next(&event, &slot, &version));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(delivery_and_recovery),
		cmocka_unit_test(changes_during_transmission),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
