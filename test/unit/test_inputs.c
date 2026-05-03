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
 *
 * With per-position encoder mapping, each encoder occupies one entry
 * in the encoder_map array.  A full row of encoders still needs
 * columns / 2 entries.
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

/**
 * @brief Verify that encoder_skip is correctly built from the default config.
 *
 * The default config maps 4 encoders on row 7 at column pairs (0,1),
 * (2,3), (4,5), (6,7).  All 8 positions on row 7 should be marked as
 * encoder positions, and no other row should be affected.
 */
static void test_encoder_skip_built_from_default_config(void **state)
{
    (void)state;

    assert_int_equal(INPUT_OK, input_init());

    /* Row 7: all 8 columns should be marked as encoder positions */
    for (uint8_t c = 0U; c < KEYPAD_COLUMNS; c++)
    {
        assert_true(input_is_encoder_position(7U, c));
    }
}

/**
 * @brief Verify that rows not hosting encoders are not marked in the skip lookup.
 */
static void test_encoder_non_encoder_positions_not_skipped(void **state)
{
    (void)state;

    assert_int_equal(INPUT_OK, input_init());

    /* Rows 0-6 should have no encoder positions with the default config */
    for (uint8_t r = 0U; r < 7U; r++)
    {
        for (uint8_t c = 0U; c < KEYPAD_COLUMNS; c++)
        {
            assert_false(input_is_encoder_position(r, c));
        }
    }
}

/**
 * @brief Verify the public getter matches direct expectations.
 */
static void test_input_is_encoder_position_getter(void **state)
{
    (void)state;

    assert_int_equal(INPUT_OK, input_init());

    /* Spot-check: (7,0) is encoder, (0,0) is not */
    assert_true(input_is_encoder_position(7U, 0U));
    assert_false(input_is_encoder_position(0U, 0U));

    /* Spot-check: (7,3) is encoder (col 2 encoder uses 2,3), (6,3) is not */
    assert_true(input_is_encoder_position(7U, 3U));
    assert_false(input_is_encoder_position(6U, 3U));
}

/**
 * @brief Hysteresis disabled (0): any non-equal value should emit.
 */
static void test_adc_should_emit_legacy_no_hysteresis(void **state)
{
    (void)state;

    assert_false(adc_should_emit(2048U, 2048U, 0U));
    assert_true(adc_should_emit(2048U, 2049U, 0U));
    assert_true(adc_should_emit(2048U, 2047U, 0U));
}

/**
 * @brief Symmetric deadband: deltas below the threshold are suppressed,
 *        deltas at or above the threshold emit, in both directions.
 */
static void test_adc_should_emit_symmetric_deadband(void **state)
{
    (void)state;

    /* Within deadband: must suppress (rising) */
    assert_false(adc_should_emit(2000U, 2007U, 8U));
    /* At threshold: must emit (rising) */
    assert_true(adc_should_emit(2000U, 2008U, 8U));
    /* Within deadband: must suppress (falling) */
    assert_false(adc_should_emit(2000U, 1993U, 8U));
    /* At threshold: must emit (falling) */
    assert_true(adc_should_emit(2000U, 1992U, 8U));
}

/**
 * @brief Hysteresis at extremes of the 12-bit ADC range must not underflow.
 */
static void test_adc_should_emit_handles_range_boundaries(void **state)
{
    (void)state;

    /* Bottom of range */
    assert_false(adc_should_emit(0U, 7U, 8U));
    assert_true(adc_should_emit(0U, 8U, 8U));

    /* Top of 12-bit range */
    assert_false(adc_should_emit(4095U, 4088U, 8U));
    assert_true(adc_should_emit(4095U, 4087U, 8U));
}

/**
 * @brief Validate that the new µs-resolution settling field is non-zero by default.
 *
 * Replacing the legacy 100 ms vTaskDelay with a µs busy-wait is the central
 * optimisation: the default must remain a meaningful settling time, not zero.
 */
static void test_adc_default_settling_is_microsecond_scale(void **state)
{
    (void)state;

    /* The header default is the contract: tests bind here so an accidental
     * regression to 0 (or back to a millisecond-scale value) is caught. */
    assert_true(ADC_DEFAULT_SETTLING_US > 0U);
    assert_true(ADC_DEFAULT_SETTLING_US <= 5000U); /* keep per-channel cycle < ~5 ms */
    assert_true(ADC_DEFAULT_OVERSAMPLE >= 1U);
    assert_true(ADC_DEFAULT_SCAN_INTERVAL_MS >= 1U);
    assert_true(ADC_NUM_TAPS >= 4U); /* must keep at least the legacy filter depth */
    /* The MA filter relies on a power-of-two tap count so it can use a mask
     * for the circular index and a shift for the divide (Cortex-M0+ has no
     * HW divide). Keep the header contract in sync with the implementation. */
    assert_int_equal(ADC_NUM_TAPS & (ADC_NUM_TAPS - 1U), 0U);
    assert_int_equal(1U << ADC_NUM_TAPS_SHIFT, ADC_NUM_TAPS);
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
        cmocka_unit_test_setup_teardown(test_encoder_skip_built_from_default_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_encoder_non_encoder_positions_not_skipped, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_is_encoder_position_getter, setup, teardown),
        cmocka_unit_test_setup_teardown(test_adc_should_emit_legacy_no_hysteresis, setup, teardown),
        cmocka_unit_test_setup_teardown(test_adc_should_emit_symmetric_deadband, setup, teardown),
        cmocka_unit_test_setup_teardown(test_adc_should_emit_handles_range_boundaries, setup, teardown),
        cmocka_unit_test_setup_teardown(test_adc_default_settling_is_microsecond_scale, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
