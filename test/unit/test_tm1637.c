/**
 * @file test_tm1637.c
 * @brief Unit tests for TM1637 driver
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "app_outputs.h"
#include "tm1637.h"

// Provide a stubbed select_interface used by tm1637 during bit-banged transfers
static output_result_t stub_select_interface(uint8_t chip_id, bool select)
{
    (void)chip_id;
    (void)select;
    return OUTPUT_OK;
}

// spi0 is provided by hardware_mocks.c, but declare here to satisfy the compiler
extern spi_inst_t *spi0;

static void test_tm1637_command_constants(void **state)
{
    (void)state;
    assert_int_equal(0x40, TM1637_CMD_DATA_WRITE);
    assert_int_equal(0x44, TM1637_CMD_FIXED_ADDR);
    assert_int_equal(0x80, TM1637_CMD_DISPLAY_OFF);
    assert_int_equal(0x88, TM1637_CMD_DISPLAY_ON);
    assert_int_equal(0xC0, TM1637_CMD_ADDR_BASE);
}

static void test_tm1637_init_set_digits_deinit(void **state)
{
    (void)state;
    // Valid pins within NUM_GPIO
    uint8_t dio_pin = 2;
    uint8_t clk_pin = 3;

    output_driver_t *drv = tm1637_init(0, &stub_select_interface, spi0, dio_pin, clk_pin);
    assert_non_null(drv);

    // Prepare 4 BCD digits and a dot at position 2
    uint8_t digits[4] = {0x0, 0x1, 0x2, 0x3};
    output_result_t r = tm1637_set_digits(drv, digits, 4, 2);
    assert_int_equal(OUTPUT_OK, r);

    // Deinitialize
    assert_int_equal(TM1637_OK, tm1637_deinit(drv));
}

static void test_tm1637_param_validation(void **state)
{
    (void)state;
    uint8_t digits[4] = {0x0, 0x1, 0x2, 0x3};

    // NULL config
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, tm1637_set_digits(NULL, digits, 4, TM1637_NO_DECIMAL_POINT));

    // Invalid length
    output_driver_t dummy = {0};
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, tm1637_set_digits(&dummy, digits, 3, TM1637_NO_DECIMAL_POINT));

    // Invalid dot position (for 4 digits, only 0..3 or NO_DECIMAL_POINT)
    assert_int_equal(OUTPUT_ERR_INVALID_PARAM, tm1637_set_digits(&dummy, digits, 4, 4));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tm1637_command_constants),
        cmocka_unit_test(test_tm1637_init_set_digits_deinit),
        cmocka_unit_test(test_tm1637_param_validation),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
