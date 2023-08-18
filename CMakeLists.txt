cmake_minimum_required(VERSION 3.13)

# Add TinyUSB lib
set(FAMILY rp2040)
set(BOARD pico_sdk)
#set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)

project(pi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pi_controller)

# Example source
target_sources(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hooks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/keypad.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
        ${CMAKE_CURRENT_SOURCE_DIR}/include/cobs.c
        )

# Example include
target_include_directories(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FreeRTOS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

#pico_enable_stdio_usb(pi_controller 1)
#pico_enable_stdio_uart(pi_controller 0)

pico_add_extra_outputs(pi_controller) 

target_link_libraries(  pi_controller 
                        pico_stdlib 
                        pico_unique_id 
                        tinyusb_device 
                        tinyusb_board 
                        FreeRTOS-Kernel 
                        FreeRTOS-Kernel-Heap1 
                        hardware_spi
                        hardware_adc
)


