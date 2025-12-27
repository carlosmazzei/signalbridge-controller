/**
 * @file test_tm1639.c
 * @brief Unit tests for tm1639 module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "tm1639.h"

// No hardware calls in these tests; rely on headers only

// Mock outputs.h and TM1639 dependencies are now included from headers

static int setup(void **state)
{
    (void) state;
    // Setup before each test
    return 0;
}

static int teardown(void **state)
{
    (void) state;
    // Cleanup after each test
    return 0;
}

static void test_tm1639_constants(void **state)
{
    (void) state;
    // Test TM1639 command constants are correctly defined
    assert_int_equal(0x02, TM1639_CMD_DATA_WRITE);
    assert_int_equal(0x22, TM1639_CMD_FIXED_ADDR);
    assert_int_equal(0x03, TM1639_CMD_ADDR_BASE);
    assert_int_equal(0x01, TM1639_CMD_DISPLAY_OFF);
    assert_int_equal(0x11, TM1639_CMD_DISPLAY_ON);
}

static void test_tm1639_buffer_constants(void **state)
{
    (void) state;
    // Test buffer size constants that actually exist in the header
    assert_int_equal(16, TM1639_MAX_DISPLAY_REGISTERS);
    assert_int_equal(8, TM1639_DIGIT_COUNT);
    assert_int_equal(16, TM1639_DISPLAY_BUFFER_SIZE);
    assert_true(TM1639_MAX_DISPLAY_REGISTERS > 0);
    assert_true(TM1639_DIGIT_COUNT > 0);
}

static void test_tm1639_mask_constants(void **state)
{
    (void) state;
    // Test mask constants
    assert_int_equal(0x80, TM1639_DECIMAL_POINT_MASK);
    assert_int_equal(0x0F, TM1639_BCD_MASK);
    assert_int_equal(9, TM1639_BCD_MAX_VALUE);
}

static void test_tm1639_result_enum(void **state)
{
    (void) state;
    // Test that result enum values are defined
    assert_int_equal(0, TM1639_OK);
    assert_true(TM1639_ERR_SPI_INIT != TM1639_OK);
    assert_true(TM1639_ERR_INVALID_PARAM != TM1639_OK);
}


int main(void) 
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_tm1639_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_buffer_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_mask_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_result_enum, setup, teardown),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
