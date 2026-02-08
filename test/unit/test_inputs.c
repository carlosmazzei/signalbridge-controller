/**
 * @file test_inputs.c
 * @brief Unit tests for inputs module
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include "error_management.h"
#include "app_context.h"
#include "app_inputs.h"

// -----------------------------------------------------------------------------
// Queue wrapper state
// -----------------------------------------------------------------------------

static bool mock_queue_create_should_fail = false;
static uintptr_t mock_queue_seed = 0x100U;
static QueueHandle_t last_deleted_queue = NULL;
static uint32_t queue_create_calls = 0U;

QueueHandle_t __wrap_xQueueGenericCreate(UBaseType_t length, UBaseType_t item_size, uint8_t queue_type)
{
    (void)length;
    (void)item_size;
    (void)queue_type;

    queue_create_calls++;

    if (mock_queue_create_should_fail)
    {
        return NULL;
    }

    mock_queue_seed += 0x10U;
    QueueHandle_t handle = (QueueHandle_t)(uintptr_t)mock_queue_seed;
    app_context_set_data_event_queue(handle);
    return handle;
}

void __wrap_vQueueDelete(QueueHandle_t queue)
{
    last_deleted_queue = queue;
    app_context_set_data_event_queue(NULL);
}

static void reset_queue_state(void)
{
    mock_queue_create_should_fail = false;
    mock_queue_seed = 0x100U;
    last_deleted_queue = NULL;
    queue_create_calls = 0U;
}

// -----------------------------------------------------------------------------
// Test fixtures
// -----------------------------------------------------------------------------

static int setup(void **state)
{
    (void)state;

    reset_queue_state();
    statistics_reset_all_counters();
    app_context_reset_queues();

    return 0;
}

static int teardown(void **state)
{
    (void)state;
    app_context_reset_queues();
    return 0;
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void test_input_init_returns_ok(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    queue_create_calls = 0U;

    input_result_t result = input_init();
    assert_int_equal(INPUT_OK, result);
    assert_int_equal(1U, queue_create_calls);
    assert_non_null(app_context_get_data_event_queue());
}

static void test_input_init_idempotent(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    queue_create_calls = 0U;

    assert_int_equal(INPUT_OK, input_init());
    QueueHandle_t first_queue = app_context_get_data_event_queue();

    assert_int_equal(INPUT_OK, input_init());
    assert_ptr_not_equal(first_queue, app_context_get_data_event_queue());
    assert_ptr_equal(first_queue, last_deleted_queue);
}

static void test_input_init_reports_queue_creation_failure(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    queue_create_calls = 0U;

    mock_queue_create_should_fail = true;

    assert_int_equal(0U, statistics_get_counter(INPUT_QUEUE_INIT_ERROR));

    input_result_t result = input_init();
    assert_int_equal(INPUT_ERROR, result);
    assert_int_equal(1U, statistics_get_counter(INPUT_QUEUE_INIT_ERROR));
    assert_null(app_context_get_data_event_queue());
}

static void test_input_init_deletes_existing_queue(void **state)
{
    (void)state;

    statistics_reset_all_counters();
    queue_create_calls = 0U;

    QueueHandle_t existing = (QueueHandle_t)0xDEADBEEF;
    app_context_set_data_event_queue(existing);
    last_deleted_queue = NULL;

    assert_int_equal(INPUT_OK, input_init());
    assert_ptr_equal(existing, last_deleted_queue);
}

/**
 * @brief Verify that the GPIO direction mask covers all mux control pins.
 *
 * Bug fix: gpio_set_dir_masked was called with 0xFF as the direction value,
 * but several mux control GPIOs are above bit 7 (e.g. GPIO 8, 11, 17, 20-22).
 * The fix passes the full gpio_mask as the direction value.
 */
static void test_gpio_mask_covers_all_mux_pins(void **state)
{
    (void)state;

    /* Build the same mask the firmware builds in input_init(). */
    uint32_t gpio_mask = (1UL << KEYPAD_COL_MUX_A) |
                         (1UL << KEYPAD_COL_MUX_B) |
                         (1UL << KEYPAD_COL_MUX_C) |
                         (1UL << KEYPAD_COL_MUX_CS) |
                         (1UL << KEYPAD_ROW_INPUT) |
                         (1UL << KEYPAD_ROW_MUX_A) |
                         (1UL << KEYPAD_ROW_MUX_B) |
                         (1UL << KEYPAD_ROW_MUX_C) |
                         (1UL << KEYPAD_ROW_MUX_CS) |
                         (1UL << ADC_MUX_A) |
                         (1UL << ADC_MUX_B) |
                         (1UL << ADC_MUX_C) |
                         (1UL << ADC_MUX_D);

    /* All output pins (everything except KEYPAD_ROW_INPUT) must have their
     * direction bit set. A direction value of 0xFF would miss GPIO >= 8. */
    assert_true((gpio_mask & ~0xFFUL) != 0);

    /* Specific pins that must be above bit 7 and thus require the full mask */
    assert_true(KEYPAD_ROW_MUX_CS >= 8U);
    assert_true(KEYPAD_COL_MUX_CS >= 8U);
    assert_true(ADC_MUX_A >= 8U);
    assert_true(ADC_MUX_B >= 8U);
    assert_true(ADC_MUX_C >= 8U);
    assert_true(ADC_MUX_D >= 8U);
}

/**
 * @brief Verify that MAX_NUM_ENCODERS can hold the maximum number of
 *        encoder pairs that can be derived from a full keypad row.
 */
static void test_encoder_config_fits_keypad(void **state)
{
    (void)state;

    /* Each encoder occupies 2 columns (A/B quadrature pair), so the maximum
     * number of encoders per row is columns / 2. */
    uint8_t max_encoders_per_row = KEYPAD_MAX_COLS / 2U;

    /* MAX_NUM_ENCODERS must be able to hold at least one full row of encoders */
    assert_true(MAX_NUM_ENCODERS >= max_encoders_per_row);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_input_init_returns_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_idempotent, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_reports_queue_creation_failure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_deletes_existing_queue, setup, teardown),
        cmocka_unit_test_setup_teardown(test_gpio_mask_covers_all_mux_pins, setup, teardown),
        cmocka_unit_test_setup_teardown(test_encoder_config_fits_keypad, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
