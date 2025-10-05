/**
 * @file test_inputs.c
 * @brief Unit tests for inputs module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "app_inputs.h"

// Mock hardware dependencies and FreeRTOS are provided by hardware_mocks.c

static int setup(void **state)
{
    (void)state;
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    return 0;
}

static void test_input_init_returns_ok(void **state)
{
    (void)state;

    input_result_t result = input_init();
    assert_int_equal(INPUT_OK, result);
}

static void test_input_init_idempotent(void **state)
{
    (void)state;

    assert_int_equal(INPUT_OK, input_init());
    assert_int_equal(INPUT_OK, input_init());
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_input_init_returns_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_idempotent, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
