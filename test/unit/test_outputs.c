/**
 * @file test_outputs.c
 * @brief Unit tests for outputs module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>

#include <cmocka.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "app_outputs.h"
#include "error_management.h"
#include "hardware/pwm.h"

// -----------------------------------------------------------------------------
// Mock state helpers
// -----------------------------------------------------------------------------

static BaseType_t mock_take_result = pdTRUE;
static BaseType_t mock_give_result = pdTRUE;
static bool mock_create_mutex_should_fail = false;

static uint32_t mock_take_calls = 0;
static uint32_t mock_give_calls = 0;
static uint32_t mock_tm1639_init_calls = 0;
static uint32_t mock_tm1637_init_calls = 0;

static output_result_t mock_set_digits_result = OUTPUT_OK;
static output_result_t mock_set_leds_result = OUTPUT_OK;

static uint8_t recorded_digits[8];
static size_t recorded_digits_len = 0;
static uint8_t recorded_dot_position = 0;
static uint32_t recorded_set_digits_calls = 0;

static uint8_t recorded_led_index = 0;
static uint8_t recorded_led_state = 0;
static uint32_t recorded_set_leds_calls = 0;

static output_driver_t mock_driver_pool[MAX_SPI_INTERFACES];
static bool mock_driver_allocated[MAX_SPI_INTERFACES];

static char mock_mutex_storage;
static SemaphoreHandle_t mock_spi_mutex = (SemaphoreHandle_t)&mock_mutex_storage;

static void clear_recorded_outputs(void)
{
    memset(recorded_digits, 0, sizeof(recorded_digits));
    recorded_digits_len = 0;
    recorded_dot_position = 0;
    recorded_led_index = 0;
    recorded_led_state = 0;
    recorded_set_digits_calls = 0;
    recorded_set_leds_calls = 0;
    mock_take_calls = 0;
    mock_give_calls = 0;
}

static void reset_mock_state(void)
{
    mock_take_result = pdTRUE;
    mock_give_result = pdTRUE;
    mock_create_mutex_should_fail = false;
    mock_set_digits_result = OUTPUT_OK;
    mock_set_leds_result = OUTPUT_OK;
    mock_tm1639_init_calls = 0;
    mock_tm1637_init_calls = 0;

    memset(mock_driver_pool, 0, sizeof(mock_driver_pool));
    memset(mock_driver_allocated, 0, sizeof(mock_driver_allocated));
    clear_recorded_outputs();
}

static output_result_t mock_driver_set_digits(output_driver_t *config,
                                              const uint8_t *digits,
                                              size_t length,
                                              uint8_t dot_position)
{
    (void)config;

    recorded_set_digits_calls++;
    recorded_digits_len = length;

    size_t copy_len = length;
    if (copy_len > sizeof(recorded_digits))
    {
        copy_len = sizeof(recorded_digits);
    }

    if ((digits != NULL) && (copy_len > 0U))
    {
        memcpy(recorded_digits, digits, copy_len);
    }

    recorded_dot_position = dot_position;
    return mock_set_digits_result;
}

static output_result_t mock_driver_set_leds(output_driver_t *config,
                                            uint8_t index,
                                            uint8_t ledstate)
{
    (void)config;

    recorded_set_leds_calls++;
    recorded_led_index = index;
    recorded_led_state = ledstate;
    return mock_set_leds_result;
}

static output_driver_t *initialise_mock_driver(uint8_t chip_id)
{
    output_driver_t *driver = &mock_driver_pool[chip_id];
    memset(driver, 0, sizeof(*driver));

    driver->chip_id = chip_id;
    driver->set_digits = mock_driver_set_digits;
    driver->set_leds = mock_driver_set_leds;

    mock_driver_allocated[chip_id] = true;
    return driver;
}

// -----------------------------------------------------------------------------
// Linker wrappers for FreeRTOS primitives and driver factories
// -----------------------------------------------------------------------------

QueueHandle_t __wrap_xQueueCreateMutex(const uint8_t ucQueueType)
{
    (void)ucQueueType;

    if (mock_create_mutex_should_fail)
    {
        return NULL;
    }

    return mock_spi_mutex;
}

BaseType_t __wrap_xQueueSemaphoreTake(QueueHandle_t xQueue, TickType_t xTicksToWait)
{
    (void)xTicksToWait;

    if (xQueue != NULL)
    {
        mock_take_calls++;
    }

    return mock_take_result;
}

BaseType_t __wrap_xQueueGenericSend(QueueHandle_t xQueue,
                                    const void *pvItemToQueue,
                                    TickType_t xTicksToWait,
                                    const BaseType_t xCopyPosition)
{
    (void)pvItemToQueue;
    (void)xTicksToWait;
    (void)xCopyPosition;

    if (xQueue != NULL)
    {
        mock_give_calls++;
    }

    return mock_give_result;
}

output_driver_t *__wrap_tm1639_init(uint8_t chip_id,
                                    output_result_t (*select_interface)(uint8_t, bool),
                                    spi_inst_t *spi,
                                    uint8_t dio_pin,
                                    uint8_t clk_pin)
{
    (void)select_interface;
    (void)spi;
    (void)dio_pin;
    (void)clk_pin;

    mock_tm1639_init_calls++;
    return initialise_mock_driver(chip_id);
}

output_driver_t *__wrap_tm1637_init(uint8_t chip_id,
                                    output_result_t (*select_interface)(uint8_t, bool),
                                    spi_inst_t *spi,
                                    uint8_t dio_pin,
                                    uint8_t clk_pin)
{
    (void)select_interface;
    (void)spi;
    (void)dio_pin;
    (void)clk_pin;

    mock_tm1637_init_calls++;
    return initialise_mock_driver(chip_id);
}

// -----------------------------------------------------------------------------
// Test fixtures
// -----------------------------------------------------------------------------

static int setup(void **state)
{
    (void)state;

    reset_mock_state();
    statistics_reset_all_counters();

    (void)output_init();
    statistics_reset_all_counters();
    clear_recorded_outputs();

    return 0;
}

static int teardown(void **state)
{
    (void)state;
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

static void test_output_init_populates_driver_pool(void **state)
{
    (void)state;

    assert_true(mock_driver_allocated[0]);
    assert_true(mock_driver_allocated[1]);
    assert_true(mock_driver_allocated[2]);
    assert_true(mock_driver_allocated[3]);

    assert_int_equal(3, mock_tm1639_init_calls);
    assert_int_equal(1, mock_tm1637_init_calls);
}

static void test_display_out_rejects_invalid_payload(void **state)
{
    (void)state;

    statistics_reset_all_counters();

    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, display_out(NULL, 0));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));

    statistics_reset_all_counters();

    uint8_t short_payload[5] = {1, 0, 0, 0, 0};
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, display_out(short_payload, sizeof(short_payload)));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));

    statistics_reset_all_counters();

    uint8_t bad_controller[6] = {9, 0, 0, 0, 0, 0};
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, display_out(bad_controller, sizeof(bad_controller)));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));
}

static void test_display_out_succeeds_and_calls_driver(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    uint8_t payload[6] = {1, 0x12, 0x34, 0x56, 0x78, 0x9A};
    assert_int_equal(OUTPUT_OK, display_out(payload, sizeof(payload)));

    assert_int_equal(1, (int)recorded_set_digits_calls);
    assert_int_equal(8, (int)recorded_digits_len);

    const uint8_t expected_digits[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    assert_memory_equal(expected_digits, recorded_digits, sizeof(expected_digits));
    assert_int_equal(payload[5], recorded_dot_position);

    assert_int_equal(1, (int)mock_take_calls);
    assert_int_equal(1, (int)mock_give_calls);
    assert_int_equal(0, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));
}

static void test_display_out_driver_error_propagates(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    mock_set_digits_result = OUTPUT_ERR_DISPLAY_OUT;

    uint8_t payload[6] = {1, 0x10, 0x00, 0x00, 0x00, 0};
    assert_int_equal(OUTPUT_ERR_DISPLAY_OUT, display_out(payload, sizeof(payload)));

    assert_int_equal(1, (int)recorded_set_digits_calls);
    assert_int_equal(0, statistics_get_counter(OUTPUT_DRIVER_INIT_ERROR));
}

static void test_display_out_semaphore_failure(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    mock_take_result = pdFALSE;

    uint8_t payload[6] = {1, 0x12, 0x34, 0x56, 0x78, 0};
    assert_int_equal(OUTPUT_ERR_SEMAPHORE, display_out(payload, sizeof(payload)));

    assert_int_equal(0, (int)recorded_set_digits_calls);
    assert_int_equal(1, (int)mock_take_calls);
    assert_int_equal(1, (int)mock_give_calls);
}

static void test_led_out_rejects_invalid_payload(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, led_out(NULL, 0));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));

    statistics_reset_all_counters();

    uint8_t short_payload[2] = {4, 0};
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, led_out(short_payload, sizeof(short_payload)));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));

    statistics_reset_all_counters();

    uint8_t wrong_device[3] = {1, 0, 0};
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, led_out(wrong_device, sizeof(wrong_device)));
    assert_int_equal(1, statistics_get_counter(OUTPUT_CONTROLLER_ID_ERROR));
}

static void test_led_out_succeeds_and_calls_driver(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    uint8_t payload[3] = {4, 5, 0xAA};
    assert_int_equal(OUTPUT_OK, led_out(payload, sizeof(payload)));

    assert_int_equal(1, (int)recorded_set_leds_calls);
    assert_int_equal(5, recorded_led_index);
    assert_int_equal(0xAA, recorded_led_state);
    assert_int_equal(0, statistics_get_counter(OUTPUT_DRIVER_INIT_ERROR));
}

static void test_led_out_missing_driver_reports_error(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    output_driver_t *driver = &mock_driver_pool[3];
    driver->set_leds = NULL;

    uint8_t payload[3] = {4, 0, 0};
    assert_int_equal(OUTPUT_ERR_DISPLAY_OUT, led_out(payload, sizeof(payload)));

    assert_int_equal(1, statistics_get_counter(OUTPUT_DRIVER_INIT_ERROR));
    assert_int_equal(0, (int)recorded_set_leds_calls);
}

static void test_led_out_semaphore_failure(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    clear_recorded_outputs();

    mock_take_result = pdFALSE;

    uint8_t payload[3] = {4, 0, 0};
    assert_int_equal(OUTPUT_ERR_SEMAPHORE, led_out(payload, sizeof(payload)));

    assert_int_equal(0, (int)recorded_set_leds_calls);
    assert_int_equal(1, (int)mock_take_calls);
    assert_int_equal(1, (int)mock_give_calls);
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
        cmocka_unit_test_setup_teardown(test_output_init_populates_driver_pool, setup, teardown),
        cmocka_unit_test_setup_teardown(test_display_out_rejects_invalid_payload, setup, teardown),
        cmocka_unit_test_setup_teardown(test_display_out_succeeds_and_calls_driver, setup, teardown),
        cmocka_unit_test_setup_teardown(test_display_out_driver_error_propagates, setup, teardown),
        cmocka_unit_test_setup_teardown(test_display_out_semaphore_failure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_led_out_rejects_invalid_payload, setup, teardown),
        cmocka_unit_test_setup_teardown(test_led_out_succeeds_and_calls_driver, setup, teardown),
        cmocka_unit_test_setup_teardown(test_led_out_missing_driver_reports_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_led_out_semaphore_failure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_pwm_duty, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
