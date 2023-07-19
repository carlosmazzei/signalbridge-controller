cmake_minimum_required(VERSION 3.13)

# Add TinyUSB lib
set(FAMILY rp2040)
set(BOARD pico_sdk)
#set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_dev_")

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(pi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pi_controller)

# Example source
target_sources(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/usb_descriptors.c
        )

# Example include
target_include_directories(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

pico_enable_stdio_usb(pi_controller 1)
pico_enable_stdio_uart(pi_controller 1)

pico_add_extra_outputs(pi_controller) 

target_link_libraries(pi_controller pico_stdlib pico_unique_id tinyusb_device tinyusb_board)


