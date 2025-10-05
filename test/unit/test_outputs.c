/**
 * @file test_outputs.c
 * @brief Unit tests for outputs module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "app_outputs.h"
#include "hardware/pwm.h"

// No hardware calls in these tests; rely on headers only

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

static void test_device_constants(void **state)
{
    (void) state;
    // Test device type constants are correctly defined
    assert_int_equal(0, DEVICE_NONE);
    assert_int_equal(1, DEVICE_GENERIC_LED);
    assert_int_equal(2, DEVICE_GENERIC_DIGIT);
    assert_int_equal(3, DEVICE_TM1639_LED);
    assert_int_equal(4, DEVICE_TM1639_DIGIT);
}

static void test_pin_constants(void **state)
{
    (void) state;
    // Test pin constants are in valid ranges
    assert_true(PWM_PIN >= 0);
    assert_true(PWM_PIN < NUM_GPIO);
    
    assert_true(SPI_MUX_A_PIN >= 0);
    assert_true(SPI_MUX_A_PIN < NUM_GPIO);
    
    assert_true(SPI_MUX_B_PIN >= 0);
    assert_true(SPI_MUX_B_PIN < NUM_GPIO);
    
    assert_true(SPI_MUX_C_PIN >= 0);
    assert_true(SPI_MUX_C_PIN < NUM_GPIO);
}

static void test_spi_frequency_constant(void **state)
{
    (void) state;
    // Test SPI frequency is reasonable (500 kHz)
    assert_int_equal(500000, SPI_FREQUENCY);
}

static void test_max_interfaces_constant(void **state)
{
    (void) state;
    // Test MAX_SPI_INTERFACES is reasonable
    assert_int_equal(8, MAX_SPI_INTERFACES);
    assert_true(MAX_SPI_INTERFACES > 0);
}

static void test_device_config_array_size(void **state)
{
    (void) state;
    // Test the device config array has the correct size
    const uint8_t device_config_map[] = DEVICE_CONFIG;
    size_t array_size = sizeof(device_config_map) / sizeof(device_config_map[0]);
    
    assert_int_equal(MAX_SPI_INTERFACES, array_size);
}

static void test_device_config_valid_values(void **state)
{
    (void) state;
    // Test that all device config values are valid device types
    const uint8_t device_config_map[] = DEVICE_CONFIG;
    
    for (size_t i = 0; i < MAX_SPI_INTERFACES; i++) {
        uint8_t device_type = device_config_map[i];
        
        // Each device type should be one of the defined constants
        assert_true(
            device_type == DEVICE_NONE ||
            device_type == DEVICE_GENERIC_LED ||
            device_type == DEVICE_GENERIC_DIGIT ||
            device_type == DEVICE_TM1639_LED ||
            device_type == DEVICE_TM1639_DIGIT ||
            device_type == DEVICE_TM1637_LED ||
            device_type == DEVICE_TM1637_DIGIT
        );
    }
}

static void test_pin_uniqueness(void **state)
{
    (void) state;
    // Test that multiplexer pins are unique (no conflicts)
    uint32_t pins[] = {SPI_MUX_A_PIN, SPI_MUX_B_PIN, SPI_MUX_C_PIN};
    size_t num_pins = sizeof(pins) / sizeof(pins[0]);
    
    // Check each pin against every other pin
    for (size_t i = 0; i < num_pins; i++) {
        for (size_t j = i + 1; j < num_pins; j++) {
            assert_int_not_equal(pins[i], pins[j]);
        }
    }
}

static void test_gpio_count_constant(void **state)
{
    (void) state;
    // Test NUM_GPIO is reasonable for Raspberry Pi Pico
    assert_int_equal(30, NUM_GPIO);
    assert_true(NUM_GPIO > 0);
}

// Wrapper for pwm_set_gpio_level to capture arguments
void __wrap_pwm_set_gpio_level(uint pin, uint16_t level)
{
    check_expected(pin);
    check_expected(level);
}

static void test_set_pwm_duty(void **state)
{
    (void) state;
    uint8_t duty = 5;
    expect_value(__wrap_pwm_set_gpio_level, pin, PWM_PIN);
    expect_value(__wrap_pwm_set_gpio_level, level, duty * duty);
    set_pwm_duty(duty);
}

int main(void) 
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_device_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pin_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_spi_frequency_constant, setup, teardown),
        cmocka_unit_test_setup_teardown(test_max_interfaces_constant, setup, teardown),
        cmocka_unit_test_setup_teardown(test_device_config_array_size, setup, teardown),
        cmocka_unit_test_setup_teardown(test_device_config_valid_values, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pin_uniqueness, setup, teardown),
        cmocka_unit_test_setup_teardown(test_gpio_count_constant, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_pwm_duty, setup, teardown),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
