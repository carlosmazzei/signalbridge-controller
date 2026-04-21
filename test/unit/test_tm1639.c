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

static void test_set_leds_column0_sets_bit7_on_every_row(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Column 0 fully lit: every row address gets its column-0 bit (0x80) set.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 0U, 0xFFU));

    for (uint8_t row = 0U; row < 8U; row++) {
        assert_int_equal(0x80, driver.active_buffer[row * 2U]);
        // Odd (unused) addresses must stay clear.
        assert_int_equal(0x00, driver.active_buffer[(row * 2U) + 1U]);
    }
}

static void test_set_leds_column7_sets_bit0_on_selected_rows(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Column 7 maps to bit 0x01; state 0x55 lights rows 0, 2, 4, 6.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 7U, 0x55U));

    assert_int_equal(0x01, driver.active_buffer[0]);
    assert_int_equal(0x00, driver.active_buffer[2]);
    assert_int_equal(0x01, driver.active_buffer[4]);
    assert_int_equal(0x00, driver.active_buffer[6]);
    assert_int_equal(0x01, driver.active_buffer[8]);
    assert_int_equal(0x00, driver.active_buffer[10]);
    assert_int_equal(0x01, driver.active_buffer[12]);
    assert_int_equal(0x00, driver.active_buffer[14]);
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

    // Light the whole column 3 (mask 0x10) across every row.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 3U, 0xFFU));
    for (uint8_t row = 0U; row < 8U; row++) {
        assert_int_equal(0x10, driver.active_buffer[row * 2U]);
    }

    // Updating column 4 (mask 0x08) with rows 0-3 lit must keep column 3 intact.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 4U, 0x0FU));
    assert_int_equal(0x10 | 0x08, driver.active_buffer[0]);
    assert_int_equal(0x10 | 0x08, driver.active_buffer[2]);
    assert_int_equal(0x10 | 0x08, driver.active_buffer[4]);
    assert_int_equal(0x10 | 0x08, driver.active_buffer[6]);
    assert_int_equal(0x10,        driver.active_buffer[8]);
    assert_int_equal(0x10,        driver.active_buffer[10]);
    assert_int_equal(0x10,        driver.active_buffer[12]);
    assert_int_equal(0x10,        driver.active_buffer[14]);
}

static void test_set_leds_clears_previously_lit_leds(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Turn every LED of column 2 on (mask 0x20) ...
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 2U, 0xFFU));

    // ... then light only the even rows of the same column; odd rows must clear.
    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 2U, 0x55U));

    assert_int_equal(0x20, driver.active_buffer[0]);
    assert_int_equal(0x00, driver.active_buffer[2]);
    assert_int_equal(0x20, driver.active_buffer[4]);
    assert_int_equal(0x00, driver.active_buffer[6]);
    assert_int_equal(0x20, driver.active_buffer[8]);
    assert_int_equal(0x00, driver.active_buffer[10]);
    assert_int_equal(0x20, driver.active_buffer[12]);
    assert_int_equal(0x00, driver.active_buffer[14]);
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_tm1639_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_buffer_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_mask_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_result_enum, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_null_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column0_sets_bit7_on_every_row, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column7_sets_bit0_on_selected_rows, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_column_out_of_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_preserves_adjacent_columns, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_clears_previously_lit_leds, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
