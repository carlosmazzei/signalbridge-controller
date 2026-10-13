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

# Test for the inbound byte-to-frame assembler (pure, no RTOS)
add_unit_test(test_encoded_framer
    test_encoded_framer.c
)

# Test for inputs module (validates config only)
add_unit_test(test_inputs
    test_inputs.c
    hardware_mocks.c
    WRAP_FUNCTIONS xQueueGenericCreate vQueueDelete vTaskDelay uxTaskGetStackHighWaterMark
)

# Test for outputs module (constant validation and simple behavior)
add_unit_test(test_outputs
    test_outputs.c
    hardware_mocks.c
    WRAP_FUNCTIONS vPortFree pwm_set_gpio_level tm1639_init tm1637_init xQueueCreateMutex xQueueSemaphoreTake xQueueGenericSend
)

# Test for tm1639 module (constant validation + set_leds behavior)
add_unit_test(test_tm1639
    test_tm1639.c
    hardware_mocks.c
)

# Test for tm1637 module (protocol/basic behavior)
add_unit_test(test_tm1637
    test_tm1637.c
    hardware_mocks.c
)

# Test for app_comm module (CDC packet sizing)
add_unit_test(test_app_comm
    test_app_comm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/app_comm.c
    hardware_mocks.c
    WRAP_FUNCTIONS xQueueGenericSend ulTaskGetRunTimeCounter ulTaskGetRunTimePercent
)

# Standalone COBS test (no hardware dependencies)
add_executable(test_cobs_standalone test_cobs_standalone.c)
target_link_libraries(test_cobs_standalone PRIVATE CMocka::cmocka)
target_include_directories(test_cobs_standalone PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMOCKA_INCLUDE_DIRS}
)
add_test(NAME test_cobs_standalone COMMAND test_cobs_standalone)

add_unit_test(test_input_delivery test_input_delivery.c)
add_unit_test(test_task_health test_task_health.c)

add_unit_test(test_usb_descriptors
    test_usb_descriptors.c hardware_mocks.c ../../src/usb_descriptors.c)
target_include_directories(test_usb_descriptors PRIVATE ${CMAKE_SOURCE_DIR}/lib/pico-sdk/lib/tinyusb/src)
target_compile_definitions(test_usb_descriptors PRIVATE CFG_TUSB_MCU=OPT_MCU_NONE CFG_TUSB_OS=OPT_OS_NONE TUP_DCD_ENDPOINT_MAX=8)

add_unit_test(test_outputs_drivers
    test_outputs_drivers.c hardware_mocks.c ../../src/app_outputs.c
    WRAP_FUNCTIONS xQueueCreateMutex xQueueSemaphoreTake xQueueGenericSend)
target_compile_definitions(test_outputs_drivers PRIVATE
    "DEVICE_CONFIG={DEVICE_TM1639_DIGIT,DEVICE_TM1637_DIGIT,DEVICE_TM1639_LED,0,0,0,0,0}")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_tooling COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_tooling.py)
set_tests_properties(test_tooling PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")

# Compile CDC callbacks against the same TinyUSB declarations as firmware.
target_include_directories(test_app_comm PRIVATE ${CMAKE_SOURCE_DIR}/lib/pico-sdk/lib/tinyusb/src)
target_compile_definitions(test_app_comm PRIVATE CFG_TUSB_MCU=OPT_MCU_NONE CFG_TUSB_OS=OPT_OS_NONE TUP_DCD_ENDPOINT_MAX=8)
