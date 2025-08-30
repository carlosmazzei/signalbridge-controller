/**
 * @file test_inputs.c
 * @brief Unit tests for inputs module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "inputs.h"

// Mock hardware dependencies and FreeRTOS are now provided by hardware_mocks.c

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

static void test_input_init_valid_config(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678, // Mock queue handle
        .encoder_mask = {true, false, true, false, false, false, false, false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_OK, result);
}

static void test_input_init_invalid_rows(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = KEYPAD_MAX_ROWS + 1,  // Invalid: too many rows
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_invalid_columns(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = KEYPAD_MAX_COLS + 1,  // Invalid: too many columns
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_invalid_adc_channels(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 17,  // Invalid: too many ADC channels (max 16)
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_zero_key_settling_time(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 0,  // Invalid: zero settling time
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_zero_adc_settling_time(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 0,  // Invalid: zero settling time
        .encoder_settling_time_ms = 20,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_zero_encoder_settling_time(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 0,  // Invalid: zero settling time
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_null_queue(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 4,
        .columns = 4,
        .key_settling_time_ms = 50,
        .adc_channels = 8,
        .adc_settling_time_ms = 10,
        .encoder_settling_time_ms = 20,
        .input_event_queue = NULL,  // Invalid: NULL queue
        .encoder_mask = {false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_INVALID_CONFIG, result);
}

static void test_input_init_boundary_valid_values(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = KEYPAD_MAX_ROWS,     // Boundary: maximum valid rows
        .columns = KEYPAD_MAX_COLS,  // Boundary: maximum valid columns
        .key_settling_time_ms = 1,   // Boundary: minimum valid settling time
        .adc_channels = 16,          // Boundary: maximum valid ADC channels
        .adc_settling_time_ms = 1,   // Boundary: minimum valid settling time
        .encoder_settling_time_ms = 1, // Boundary: minimum valid settling time
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {true, true, true, true, true, true, true, true}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_OK, result);
}

static void test_input_init_minimal_valid_config(void **state)
{
    (void) state;
    input_config_t config = {
        .rows = 1,
        .columns = 1,
        .key_settling_time_ms = 1,
        .adc_channels = 1,
        .adc_settling_time_ms = 1,
        .encoder_settling_time_ms = 1,
        .input_event_queue = (QueueHandle_t)0x12345678,
        .encoder_mask = {false, false, false, false, false, false, false, false}
    };
    
    input_result_t result = input_init(&config);
    assert_int_equal(INPUT_OK, result);
}

// Wrapper to intercept gpio_put calls
void __wrap_gpio_put(uint pin, bool value)
{
    check_expected(pin);
    check_expected(value);
}

static void test_adc_mux_select(void **state)
{
    (void) state;
    // Expect each pin to be set according to channel bits (0x0F)
    expect_value(__wrap_gpio_put, pin, ADC_MUX_A);
    expect_value(__wrap_gpio_put, value, true);
    expect_value(__wrap_gpio_put, pin, ADC_MUX_B);
    expect_value(__wrap_gpio_put, value, true);
    expect_value(__wrap_gpio_put, pin, ADC_MUX_C);
    expect_value(__wrap_gpio_put, value, true);
    expect_value(__wrap_gpio_put, pin, ADC_MUX_D);
    expect_value(__wrap_gpio_put, value, true);
    adc_mux_select(0x0F);
}

int main(void) 
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_input_init_valid_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_invalid_rows, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_invalid_columns, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_invalid_adc_channels, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_zero_key_settling_time, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_zero_adc_settling_time, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_zero_encoder_settling_time, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_null_queue, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_boundary_valid_values, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_minimal_valid_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_adc_mux_select, setup, teardown),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}