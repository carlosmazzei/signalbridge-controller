/**
 * @file test_cobs_example.c
 * @brief Example CMocka test for COBS module demonstrating best practices
 * 
 * This shows the proper CMocka pattern for testing without hardware mocks
 * since COBS is a pure algorithm with no hardware dependencies.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

// Include the module under test
#include "cobs.h"

// Test fixtures and setup
static int setup(void **state) {
    (void) state; // unused parameter
    return 0;
}

static int teardown(void **state) {
    (void) state; // unused parameter  
    return 0;
}

// Test cases
static void test_cobs_encode_basic(void **state) {
    (void) state; // unused parameter
    
    uint8_t input[] = {0x11, 0x22, 0x00, 0x33};
    uint8_t output[10];
    size_t result_len;
    
    cobs_result_t result = cobs_encode(input, sizeof(input), output, sizeof(output), &result_len);
    
    assert_int_equal(result, COBS_RESULT_SUCCESS);
    assert_int_equal(result_len, 5);
    // Verify encoded content based on COBS algorithm
    assert_int_equal(output[0], 0x03); // Distance to first zero
    assert_int_equal(output[1], 0x11);
    assert_int_equal(output[2], 0x22);
    assert_int_equal(output[3], 0x02); // Distance to next zero (end)
    assert_int_equal(output[4], 0x33);
}

static void test_cobs_decode_basic(void **state) {
    (void) state; // unused parameter
    
    uint8_t input[] = {0x03, 0x11, 0x22, 0x02, 0x33};
    uint8_t output[10];
    size_t result_len;
    
    cobs_result_t result = cobs_decode(input, sizeof(input), output, sizeof(output), &result_len);
    
    assert_int_equal(result, COBS_RESULT_SUCCESS);
    assert_int_equal(result_len, 4);
    assert_int_equal(output[0], 0x11);
    assert_int_equal(output[1], 0x22);
    assert_int_equal(output[2], 0x00);
    assert_int_equal(output[3], 0x33);
}

static void test_cobs_encode_buffer_too_small(void **state) {
    (void) state; // unused parameter
    
    uint8_t input[] = {0x11, 0x22, 0x33};
    uint8_t output[2]; // Too small
    size_t result_len;
    
    cobs_result_t result = cobs_encode(input, sizeof(input), output, sizeof(output), &result_len);
    
    assert_int_equal(result, COBS_RESULT_ERROR_BUFFER_TOO_SMALL);
}

// Test runner
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_cobs_encode_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cobs_decode_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cobs_encode_buffer_too_small, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}