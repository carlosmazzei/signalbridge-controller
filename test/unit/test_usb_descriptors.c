#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <cmocka.h>
#include "tusb.h"

void pico_get_unique_board_id_string(char *id, size_t len)
{
	const char serial[] = "0123456789abcdef";
	if (len >= sizeof(serial))
		memcpy(id, serial, sizeof(serial));
}

static void configuration_consistency(void **state)
{
	(void)state;
	const uint8_t *descriptor = tud_descriptor_configuration_cb(0U);
	size_t length = descriptor[2] | ((size_t)descriptor[3] << 8U);
	assert_int_equal(length, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN);
	assert_int_equal(descriptor[4], 2U);
	unsigned interfaces = 0U;
	unsigned endpoints = 0U;
	size_t offset = 0U;
	while (offset < length)
	{
		uint8_t size = descriptor[offset];
		assert_true(size >= 2U);
		assert_true(offset + size <= length);
		if (descriptor[offset + 1U] == TUSB_DESC_INTERFACE)
			interfaces++;
		if (descriptor[offset + 1U] == TUSB_DESC_ENDPOINT)
			endpoints++;
		offset += size;
	}
	assert_int_equal(offset, length);
	assert_int_equal(interfaces, descriptor[4]);
	assert_int_equal(endpoints, 3U);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(configuration_consistency)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
