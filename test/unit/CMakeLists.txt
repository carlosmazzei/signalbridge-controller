# Unit Tests using CMocka best practices

# Test for error_management module
add_unit_test(test_error_management 
    test_error_management.c
    WRAP_FUNCTIONS
        gpio_init gpio_set_dir gpio_put
        watchdog_enable watchdog_update
        time_us_32
)

# Test for COBS encoder/decoder (no hardware dependencies)
add_unit_test(test_cobs 
    test_cobs.c
)

# Test for inputs module  
add_unit_test(test_inputs
    test_inputs.c
    WRAP_FUNCTIONS
        gpio_init_mask gpio_set_dir_masked gpio_set_dir
        gpio_put_masked gpio_put gpio_get
        adc_gpio_init
        watchdog_update
)

# Test for outputs module
add_unit_test(test_outputs
    test_outputs.c
    WRAP_FUNCTIONS
        gpio_init gpio_set_dir gpio_put
        gpio_set_function gpio_get_function
        spi_init spi_set_format spi_write_blocking
        pwm_gpio_to_slice_num pwm_get_default_config
        pwm_config_set_clkdiv pwm_init pwm_set_gpio_level
        sleep_us
)

# Test for tm1639 module
add_unit_test(test_tm1639
    test_tm1639.c
    WRAP_FUNCTIONS
        gpio_set_function gpio_set_dir gpio_put gpio_get
        gpio_pull_up
        spi_write_blocking
        sleep_us
)

# Standalone COBS test (no hardware dependencies)
add_executable(test_cobs_standalone test_cobs_standalone.c)
target_link_libraries(test_cobs_standalone ${CMOCKA_LIBRARIES})
target_include_directories(test_cobs_standalone PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMOCKA_INCLUDE_DIRS}
)
add_test(NAME test_cobs_standalone COMMAND test_cobs_standalone)

