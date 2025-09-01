/**
 * @file test_cobs_standalone.c
 * @brief Standalone CMocka test for COBS module
 * 
 * This test compiles COBS independently to verify the static library
 * approach works without hardware dependencies.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

// Include the COBS header first
#include "cobs.h"

// Include the COBS source directly to avoid library dependencies  
#include "../../src/cobs.c"

// Test fixtures
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
    
    size_t result_len = cobs_encode(input, sizeof(input), output);
    
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
    
    size_t result_len = cobs_decode(input, sizeof(input), output);
    
    assert_int_equal(result_len, 4);
    assert_int_equal(output[0], 0x11);
    assert_int_equal(output[1], 0x22);
    assert_int_equal(output[2], 0x00);
    assert_int_equal(output[3], 0x33);
}

static void test_cobs_roundtrip(void **state) {
    (void) state; // unused parameter
    
    uint8_t original[] = {0x11, 0x22, 0x00, 0x33, 0x44, 0x00, 0x55};
    uint8_t encoded[20];
    uint8_t decoded[20];
    
    // Encode
    size_t encoded_len = cobs_encode(original, sizeof(original), encoded);
    assert_true(encoded_len > 0);
    
    // Decode
    size_t decoded_len = cobs_decode(encoded, encoded_len, decoded);
    assert_int_equal(decoded_len, sizeof(original));
    
    // Verify roundtrip
    assert_memory_equal(original, decoded, sizeof(original));
}

// Test runner
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_cobs_encode_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cobs_decode_basic, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cobs_roundtrip, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}