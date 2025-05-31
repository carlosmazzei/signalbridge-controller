add_executable(pi_controller)

# Source files
target_sources(pi_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hooks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cobs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/inputs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/outputs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tm1639.c
)

target_compile_definitions(pi_controller PRIVATE
    configNUMBER_OF_CORES=2
)

# Instructions to enable/disable debugging using printf commands
pico_enable_stdio_usb(pi_controller 0)
pico_enable_stdio_uart(pi_controller 0)

pico_add_extra_outputs(pi_controller) 

target_include_directories(pi_controller PUBLIC
    ${FREERTOS_KERNEL_PATH}/include
    ${PICO_SDK_PATH}/src/common/pico_stdlib/include
)

# Link required libraries
target_link_libraries(pi_controller
    pico_controller_headers
    pico_stdlib 
    pico_multicore
    pico_unique_id 
    tinyusb_device 
    tinyusb_board 
    hardware_adc
    hardware_spi
    hardware_pwm
    hardware_pio
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

