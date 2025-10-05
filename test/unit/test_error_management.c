/**
 * @file test_error_management.c
 * @brief Unit tests for error management module statistics functions
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "error_management.h"

typedef struct {
    uint32_t scratch[8];
} watchdog_hw_t;

extern watchdog_hw_t *watchdog_hw;
extern void mock_watchdog_set_reboot_flag(bool flag);
extern void mock_time_config(uint32_t initial_value, uint32_t step);

static int setup(void **state)
{
    (void) state;
    // Reset statistics counters
    statistics_reset_all_counters();
    statistics_clear_error();
    mock_time_config(0, 0);
    mock_watchdog_set_reboot_flag(false);
    return 0;
}

static int teardown(void **state)
{
    (void) state;
    // Cleanup after each test
    return 0;
}

static void test_statistics_increment_counter(void **state)
{
    (void) state;
    assert_int_equal(0, statistics_get_counter(QUEUE_SEND_ERROR));
    
    statistics_increment_counter(QUEUE_SEND_ERROR);
    assert_int_equal(1, statistics_get_counter(QUEUE_SEND_ERROR));
    
    statistics_increment_counter(QUEUE_SEND_ERROR);
    assert_int_equal(2, statistics_get_counter(QUEUE_SEND_ERROR));
}

static void test_statistics_add_to_counter(void **state)
{
    (void) state;
    assert_int_equal(0, statistics_get_counter(BYTES_SENT));
    
    statistics_add_to_counter(BYTES_SENT, 100);
    assert_int_equal(100, statistics_get_counter(BYTES_SENT));
    
    statistics_add_to_counter(BYTES_SENT, 50);
    assert_int_equal(150, statistics_get_counter(BYTES_SENT));
}

static void test_statistics_set_counter(void **state)
{
    (void) state;
    statistics_set_counter(BYTES_RECEIVED, 500);
    assert_int_equal(500, statistics_get_counter(BYTES_RECEIVED));
    
    statistics_set_counter(BYTES_RECEIVED, 1000);
    assert_int_equal(1000, statistics_get_counter(BYTES_RECEIVED));
}

static void test_statistics_reset_all_counters(void **state)
{
    (void) state;
    // Set some counters to non-zero values
    statistics_increment_counter(QUEUE_SEND_ERROR);
    statistics_add_to_counter(BYTES_SENT, 100);
    statistics_set_counter(BYTES_RECEIVED, 200);
    
    // Verify they are non-zero
    assert_int_not_equal(0, statistics_get_counter(QUEUE_SEND_ERROR));
    assert_int_not_equal(0, statistics_get_counter(BYTES_SENT));
    assert_int_not_equal(0, statistics_get_counter(BYTES_RECEIVED));
    
    // Reset all counters
    statistics_reset_all_counters();
    
    // Verify all are now zero
    assert_int_equal(0, statistics_get_counter(QUEUE_SEND_ERROR));
    assert_int_equal(0, statistics_get_counter(BYTES_SENT));
    assert_int_equal(0, statistics_get_counter(BYTES_RECEIVED));
}

static void test_statistics_error_state_initial(void **state)
{
    (void) state;
    // Test initial state after reset
    assert_false(statistics_is_error_state());
    assert_int_equal(ERROR_NONE, statistics_get_error_type());
}

static void test_error_management_record_recoverable_sets_state(void **state)
{
    (void) state;

    error_management_record_recoverable(ERROR_WATCHDOG_TIMEOUT);

    assert_true(statistics_is_error_state());
    assert_int_equal(ERROR_WATCHDOG_TIMEOUT, statistics_get_error_type());
    assert_int_equal(1, statistics_get_counter(WATCHDOG_ERROR));
}

static void test_error_management_record_recoverable_heap_counter(void **state)
{
    (void) state;

    error_management_record_recoverable(ERROR_RESOURCE_ALLOCATION);

    assert_true(statistics_is_error_state());
    assert_int_equal(1, statistics_get_counter(RECOVERY_HEAP_ERROR));
    assert_int_equal(ERROR_RESOURCE_ALLOCATION, statistics_get_error_type());
}

static void test_error_management_record_fatal_clears_state(void **state)
{
    (void) state;

    error_management_record_recoverable(ERROR_WATCHDOG_TIMEOUT);
    assert_true(statistics_is_error_state());

    error_management_record_fatal(ERROR_FREERTOS_STACK);

    assert_false(statistics_is_error_state());
    assert_int_equal(ERROR_FREERTOS_STACK, statistics_get_error_type());
}

static void test_error_management_is_recoverable_matrix(void **state)
{
    (void) state;

    assert_true(error_management_is_recoverable(ERROR_WATCHDOG_TIMEOUT));
    assert_true(error_management_is_recoverable(ERROR_RESOURCE_ALLOCATION));
    assert_false(error_management_is_recoverable(ERROR_PICO_SDK_PANIC));
    assert_false(error_management_is_recoverable(ERROR_SCHEDULER_FAILED));
}

static void test_show_error_for_duration_advances_time(void **state)
{
    (void) state;

    error_management_record_recoverable(ERROR_WATCHDOG_TIMEOUT);
    mock_time_config(0, 5000U);

    show_error_for_duration_ms(10U);

    assert_true(statistics_is_error_state());
}

static void test_setup_watchdog_with_error_detection_updates_counters(void **state)
{
    (void) state;

    watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = 5U;
    mock_watchdog_set_reboot_flag(false);

    setup_watchdog_with_error_detection(1000U);

    assert_int_equal(5U, statistics_get_counter(WATCHDOG_ERROR));
    assert_int_equal(5U, watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG]);

    statistics_reset_all_counters();
    watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG] = 1U;
    mock_watchdog_set_reboot_flag(true);

    setup_watchdog_with_error_detection(1000U);

    assert_int_equal(2U, statistics_get_counter(WATCHDOG_ERROR));
    assert_int_equal(2U, watchdog_hw->scratch[WATCHDOG_ERROR_COUNT_REG]);
    mock_watchdog_set_reboot_flag(false);
}

static void test_counter_bounds(void **state)
{
    (void) state;
    // Test with different counter enum values
    statistics_increment_counter(QUEUE_RECEIVE_ERROR);
    assert_int_equal(1, statistics_get_counter(QUEUE_RECEIVE_ERROR));
    
    statistics_increment_counter(CDC_QUEUE_SEND_ERROR);
    assert_int_equal(1, statistics_get_counter(CDC_QUEUE_SEND_ERROR));
    
    // Test with last counter enum value (NUM_STATISTICS_COUNTERS - 1)
    if (NUM_STATISTICS_COUNTERS > 0) {
        statistics_increment_counter(NUM_STATISTICS_COUNTERS - 1);
        assert_int_equal(1, statistics_get_counter(NUM_STATISTICS_COUNTERS - 1));
    }
}

static void test_multiple_counter_operations(void **state)
{
    (void) state;
    // Test complex operations on multiple counters
    statistics_set_counter(DISPLAY_OUT_ERROR, 10);
    statistics_add_to_counter(DISPLAY_OUT_ERROR, 5);
    statistics_increment_counter(DISPLAY_OUT_ERROR);
    
    assert_int_equal(16, statistics_get_counter(DISPLAY_OUT_ERROR));
    
    // Verify other counters are unaffected
    assert_int_equal(0, statistics_get_counter(LED_OUT_ERROR));
    assert_int_equal(0, statistics_get_counter(WATCHDOG_ERROR));
}

static void test_counter_overflow_behavior(void **state)
{
    (void) state;
    // Test behavior near uint32_t limits
    uint32_t large_value = 0xFFFFFFFF - 10;
    statistics_set_counter(UNKNOWN_CMD_ERROR, large_value);
    
    assert_int_equal(large_value, statistics_get_counter(UNKNOWN_CMD_ERROR));
    
    // Add a small value that would cause overflow
    statistics_add_to_counter(UNKNOWN_CMD_ERROR, 20);
    
    // Should wrap around (standard uint32_t behavior)
    uint32_t expected = large_value + 20; // This will overflow
    assert_int_equal(expected, statistics_get_counter(UNKNOWN_CMD_ERROR));
}

int main(void) 
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_statistics_increment_counter, setup, teardown),
        cmocka_unit_test_setup_teardown(test_statistics_add_to_counter, setup, teardown),
        cmocka_unit_test_setup_teardown(test_statistics_set_counter, setup, teardown),
        cmocka_unit_test_setup_teardown(test_statistics_reset_all_counters, setup, teardown),
        cmocka_unit_test_setup_teardown(test_statistics_error_state_initial, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_management_record_recoverable_sets_state, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_management_record_recoverable_heap_counter, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_management_record_fatal_clears_state, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_management_is_recoverable_matrix, setup, teardown),
        cmocka_unit_test_setup_teardown(test_show_error_for_duration_advances_time, setup, teardown),
        cmocka_unit_test_setup_teardown(test_setup_watchdog_with_error_detection_updates_counters, setup, teardown),
        cmocka_unit_test_setup_teardown(test_counter_bounds, setup, teardown),
        cmocka_unit_test_setup_teardown(test_multiple_counter_operations, setup, teardown),
        cmocka_unit_test_setup_teardown(test_counter_overflow_behavior, setup, teardown),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
