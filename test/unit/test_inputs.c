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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_input_init_returns_ok, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_idempotent, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_reports_queue_creation_failure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_input_init_deletes_existing_queue, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
