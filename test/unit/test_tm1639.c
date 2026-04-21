/**
 * @file test_tm1639.c
 * @brief Unit tests for tm1639 module
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include "app_outputs.h"
#include "tm1639.h"

// spi0 is provided by hardware_mocks.c; declare to satisfy the compiler.
extern spi_inst_t *spi0;

static output_result_t stub_select_interface(uint8_t chip_id, bool select)
{
    (void)chip_id;
    (void)select;
    return OUTPUT_OK;
}

static void init_stub_driver(output_driver_t *driver)
{
    (void)memset(driver, 0, sizeof(*driver));
    driver->chip_id = 0U;
    driver->select_interface = stub_select_interface;
    driver->spi = spi0;
    driver->dio_pin = 0U;
    driver->clk_pin = 1U;
    driver->buffer_modified = false;
    driver->brightness = 7U;
    driver->display_on = true;
}

static int setup(void **state)
{
    (void) state;
    return 0;
}

static int teardown(void **state)
{
    (void) state;
    return 0;
}

static void test_tm1639_constants(void **state)
{
    (void) state;
    // Test TM1639 command constants are correctly defined
    assert_int_equal(0x02, TM1639_CMD_DATA_WRITE);
    assert_int_equal(0x22, TM1639_CMD_FIXED_ADDR);
    assert_int_equal(0x03, TM1639_CMD_ADDR_BASE);
    assert_int_equal(0x01, TM1639_CMD_DISPLAY_OFF);
    assert_int_equal(0x11, TM1639_CMD_DISPLAY_ON);
}

static void test_tm1639_buffer_constants(void **state)
{
    (void) state;
    // Test buffer size constants that actually exist in the header
    assert_int_equal(16, TM1639_MAX_DISPLAY_REGISTERS);
    assert_int_equal(8, TM1639_DIGIT_COUNT);
    assert_int_equal(16, TM1639_DISPLAY_BUFFER_SIZE);
    assert_true(TM1639_MAX_DISPLAY_REGISTERS > 0);
    assert_true(TM1639_DIGIT_COUNT > 0);
}

static void test_tm1639_mask_constants(void **state)
{
    (void) state;
    // Test mask constants
    assert_int_equal(0x80, TM1639_DECIMAL_POINT_MASK);
    assert_int_equal(0x0F, TM1639_BCD_MASK);
    assert_int_equal(9, TM1639_BCD_MAX_VALUE);
}

static void test_tm1639_result_enum(void **state)
{
    (void) state;
    // Test that result enum values are defined
    assert_int_equal(0, TM1639_OK);
    assert_true(TM1639_ERR_SPI_INIT != TM1639_OK);
    assert_true(TM1639_ERR_INVALID_PARAM != TM1639_OK);
}

static void test_set_leds_rejects_null_config(void **state)
{
    (void) state;
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, tm1639_set_leds(NULL, 0U, 0xFFU));
}

static void test_set_leds_column0_splits_nibbles_across_addr_pair(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Column 0 owns addresses 0 and 1. 0xFF -> low nibble on addr 0,
    // high nibble on addr 1; the upper nibble of each byte stays clear.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 0U, 0xFFU));

    assert_int_equal(0x0F, driver.active_buffer[0]);
    assert_int_equal(0x0F, driver.active_buffer[1]);
}

static void test_set_leds_column7_splits_nibbles_across_addr_pair(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Column 7 owns addresses 14 and 15. 0xA5 -> low=0x05, high=0x0A.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 7U, 0xA5U));

    assert_int_equal(0x05, driver.active_buffer[14]);
    assert_int_equal(0x0A, driver.active_buffer[15]);
}

static void test_set_leds_rejects_column_out_of_range(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    assert_int_equal(OUTPUT_ERR_DISPLAY_OUT, tm1639_set_leds(&driver, 8U, 0xFFU));

    // Buffer must remain untouched when the column is rejected.
    for (size_t i = 0U; i < sizeof(driver.active_buffer); i++) {
        assert_int_equal(0x00, driver.active_buffer[i]);
    }
}

static void test_set_leds_preserves_adjacent_columns(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Fill column 3 (addresses 6, 7) with all LEDs lit.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 3U, 0xFFU));
    assert_int_equal(0x0F, driver.active_buffer[6]);
    assert_int_equal(0x0F, driver.active_buffer[7]);

    // Updating column 4 (addresses 8, 9) with 0x0F must not touch
    // addresses 6 or 7.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 4U, 0x0FU));
    assert_int_equal(0x0F, driver.active_buffer[6]);
    assert_int_equal(0x0F, driver.active_buffer[7]);
    assert_int_equal(0x0F, driver.active_buffer[8]);
    assert_int_equal(0x00, driver.active_buffer[9]);
}

static void test_set_leds_clears_previously_lit_leds(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Light every LED of column 2, then flip the pattern: the buffer must
    // match the second write (no sticky bits from the first one).
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 2U, 0xFFU));
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 2U, 0x50U));

    assert_int_equal(0x00, driver.active_buffer[4]);
    assert_int_equal(0x05, driver.active_buffer[5]);
}

static void test_set_leds_example_from_protocol_doc(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Documented example: 0x00 0xFF must light every LED of the first column.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 0x00U, 0xFFU));

    assert_int_equal(0x0F, driver.active_buffer[0]);
    assert_int_equal(0x0F, driver.active_buffer[1]);
    // Remaining addresses stay dark.
    for (size_t i = 2U; i < sizeof(driver.active_buffer); i++) {
        assert_int_equal(0x00, driver.active_buffer[i]);
    }
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_tm1639_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_buffer_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_mask_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_result_enum, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_null_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column0_splits_nibbles_across_addr_pair, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column7_splits_nibbles_across_addr_pair, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_column_out_of_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_preserves_adjacent_columns, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_clears_previously_lit_leds, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_example_from_protocol_doc, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
