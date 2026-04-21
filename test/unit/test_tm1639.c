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

static void test_set_leds_column0_writes_addr0(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 0U, 0xAAU));

    // GRID1 SEG1-SEG8 lives at the even address; SEG9/SEG10 must stay clear.
    assert_int_equal(0xAA, driver.active_buffer[0]);
    assert_int_equal(0x00, driver.active_buffer[1]);
}

static void test_set_leds_column7_writes_addr14(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 7U, 0x55U));

    assert_int_equal(0x55, driver.active_buffer[14]);
    assert_int_equal(0x00, driver.active_buffer[15]);
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

    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 3U, 0xFFU));
    assert_int_equal(0xFF, driver.active_buffer[6]);

    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 4U, 0x0FU));
    assert_int_equal(0xFF, driver.active_buffer[6]);
    assert_int_equal(0x0F, driver.active_buffer[8]);
}

static void test_set_leds_clears_odd_byte_from_previous_state(void **state)
{
    (void) state;
    output_driver_t driver;
    init_stub_driver(&driver);

    // Simulate a stale SEG9/SEG10 value on GRID2 left over from a previous path.
    driver.prep_buffer[3] = 0xFFU;
    driver.active_buffer[3] = 0xFFU;

    assert_int_equal(OUTPUT_OK, tm1639_set_leds(&driver, 1U, 0x11U));

    assert_int_equal(0x11, driver.active_buffer[2]);
    assert_int_equal(0x00, driver.active_buffer[3]);
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_tm1639_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_buffer_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_mask_constants, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tm1639_result_enum, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_null_config, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column0_writes_addr0, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_column7_writes_addr14, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_rejects_column_out_of_range, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_preserves_adjacent_columns, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_leds_clears_odd_byte_from_previous_state, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
