#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "app_outputs.h"
#include "mock_spi_capture.h"

static char mutex_storage;
QueueHandle_t __wrap_xQueueCreateMutex(uint8_t type)
{
	(void)type;
	return (QueueHandle_t)&mutex_storage;
}
BaseType_t __wrap_xQueueSemaphoreTake(QueueHandle_t queue, TickType_t ticks)
{
	(void)queue; (void)ticks;
	return pdTRUE;
}
BaseType_t __wrap_xQueueGenericSend(QueueHandle_t queue, const void *item, TickType_t ticks, BaseType_t position)
{
	(void)queue; (void)item; (void)ticks; (void)position;
	return pdTRUE;
}

static void mixed_drivers(void **state)
{
	(void)state;
	assert_int_equal(output_init(), OUTPUT_OK);
	uint8_t digits[] = {0x20U, 0x12U, 0x34U, 0x56U, 0x78U, 0xFFU};
	mock_spi_reset();
	assert_int_equal(display_out(digits, sizeof(digits)), OUTPUT_OK); // real TM1639
	assert_true(mock_spi_call_count() > 0U);
	digits[0] = 0x40U;
	assert_int_equal(display_out(digits, sizeof(digits)), OUTPUT_OK); // real TM1637, 4 digits
	uint8_t leds[] = {3U, 0U, 0xAAU};
	assert_int_equal(led_out(leds, sizeof(leds)), OUTPUT_OK);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(mixed_drivers)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
