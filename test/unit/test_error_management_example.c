/**
 * @file test_error_management_example.c
 * @brief Example CMocka test with hardware mocking using --wrap
 * 
 * This demonstrates proper CMocka patterns for testing hardware-dependent code
 * using the --wrap linker feature and CMocka's expect/check functions.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdint.h>
#include <stdbool.h>

// Include the module under test
#include "error_management.h"

// Mock Pico SDK types (minimal definitions needed)
typedef unsigned int uint;

// Hardware mock implementations using CMocka patterns
void __wrap_gpio_init(uint pin) {
    check_expected(pin);
}

void __wrap_gpio_set_dir(uint pin, bool out) {
    check_expected(pin);
    check_expected(out);
}

void __wrap_gpio_put(uint pin, bool value) {
    check_expected(pin);
    check_expected(value);
}

void __wrap_watchdog_enable(uint32_t delay_ms, bool pause_on_debug) {
    check_expected(delay_ms);
    check_expected(pause_on_debug);
}

void __wrap_watchdog_update(void) {
    function_called();
}

uint32_t __wrap_time_us_32(void) {
    return (uint32_t)mock();
}

// Test fixtures
static int setup(void **state) {
    (void) state;
    return 0;
}

static int teardown(void **state) {
    (void) state;
    return 0;
}

// Test cases
static void test_setup_watchdog_with_error_detection(void **state) {
    (void) state;
    
    // Set expectations for hardware calls
    expect_value(__wrap_gpio_init, pin, ERROR_LED_PIN);
    expect_value(__wrap_gpio_set_dir, pin, ERROR_LED_PIN);
    expect_value(__wrap_gpio_set_dir, out, true); // GPIO_OUT
    expect_value(__wrap_gpio_put, pin, ERROR_LED_PIN);
    expect_value(__wrap_gpio_put, value, false);
    expect_value(__wrap_watchdog_enable, delay_ms, 5000);
    expect_value(__wrap_watchdog_enable, pause_on_debug, true);
    
    // Call the function under test
    setup_watchdog_with_error_detection(5000);
    
    // CMocka automatically verifies all expectations were met
}

static void test_update_watchdog_safe_normal_operation(void **state) {
    (void) state;
    
    // Expect watchdog_update to be called once
    expect_function_call(__wrap_watchdog_update);
    
    // Call the function under test
    update_watchdog_safe();
    
    // CMocka verifies the expected call occurred
}

static void test_signal_error_blinks_led(void **state) {
    (void) state;
    
    // Mock time progression for LED blinking
    will_return(__wrap_time_us_32, 0);      // First call
    will_return(__wrap_time_us_32, 500000); // 500ms later
    will_return(__wrap_time_us_32, 1000000);// 1s later
    
    // Expect LED to turn on, then off during blink
    expect_value(__wrap_gpio_put, pin, ERROR_LED_PIN);
    expect_value(__wrap_gpio_put, value, true);  // LED on
    expect_value(__wrap_gpio_put, pin, ERROR_LED_PIN);
    expect_value(__wrap_gpio_put, value, false); // LED off
    
    // Call the function under test
    signal_error(ERROR_COMMUNICATION_TIMEOUT);
}

static void test_error_count_increments(void **state) {
    (void) state;
    
    // This test would verify error counting logic
    // Implementation depends on how error counting is exposed
    // For now, just test that the function doesn't crash
    signal_error(ERROR_INVALID_COMMAND);
}

// Test runner
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_setup_watchdog_with_error_detection, setup, teardown),
        cmocka_unit_test_setup_teardown(test_update_watchdog_safe_normal_operation, setup, teardown),
        cmocka_unit_test_setup_teardown(test_signal_error_blinks_led, setup, teardown),
        cmocka_unit_test_setup_teardown(test_error_count_increments, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}