#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "task_health.h"

static void partial_hang(void **state)
{
	(void)state;
	task_health_reset(0U);
	for (unsigned i = 0U; i < NUM_TASKS; i++)
		if (i != KEYPAD_TASK)
			task_health_progress((task_enum_t)i, TASK_HEALTH_TIMEOUT_US);
	assert_false(task_health_check(TASK_HEALTH_TIMEOUT_US));
	task_health_progress(KEYPAD_TASK, TASK_HEALTH_TIMEOUT_US);
	assert_true(task_health_check(TASK_HEALTH_TIMEOUT_US));
}

static void idle_and_wrap(void **state)
{
	(void)state;
	task_health_reset(UINT32_MAX - 100U);
	assert_true(task_health_check(50U));
	// A concurrent core may publish after the supervisor sampled now_us.
	task_health_progress(KEYPAD_TASK, 51U);
	assert_true(task_health_check(50U));
	for (unsigned i = 0U; i < NUM_TASKS; i++) task_health_wait((task_enum_t)i);
	assert_true(task_health_check(TASK_HEALTH_TIMEOUT_US));
	task_health_progress(CDC_TASK, 100U);
	assert_false(task_health_check(TASK_HEALTH_TIMEOUT_US + 100U));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(partial_hang), cmocka_unit_test(idle_and_wrap),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
