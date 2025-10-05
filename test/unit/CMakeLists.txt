# Unit Tests using CMocka best practices

# Test for error_management module (no hardware interaction in these tests)
add_unit_test(test_error_management 
    test_error_management.c
    hardware_mocks.c
)

# Test for COBS encoder/decoder (no hardware dependencies)
add_unit_test(test_cobs 
    test_cobs.c
)

# Test for inputs module (validates config only)
add_unit_test(test_inputs
    test_inputs.c
    hardware_mocks.c
)

# Test for outputs module (constant validation and simple behavior)
add_unit_test(test_outputs
    test_outputs.c
    hardware_mocks.c
    WRAP_FUNCTIONS pwm_set_gpio_level
)

# Test for tm1639 module (constant validation)
add_unit_test(test_tm1639
    test_tm1639.c
)

# Test for tm1637 module (protocol/basic behavior)
add_unit_test(test_tm1637
    test_tm1637.c
    hardware_mocks.c
)

# Standalone COBS test (no hardware dependencies)
add_executable(test_cobs_standalone test_cobs_standalone.c)
target_link_libraries(test_cobs_standalone ${CMOCKA_LIBRARIES})
target_include_directories(test_cobs_standalone PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMOCKA_INCLUDE_DIRS}
)
add_test(NAME test_cobs_standalone COMMAND test_cobs_standalone)
