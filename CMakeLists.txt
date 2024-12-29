cmake_minimum_required(VERSION 3.14)

set(FAMILY rp2040)
set(BOARD pico_sdk)

# Pull in SDK (must be before project)
include(lib/pico-sdk/pico_sdk_init.cmake)

# Pull in FreeRTOS
set(FREERTOS_KERNEL_PATH "../lib/FreeRTOS-Kernel/")
include(FreeRTOS_Kernel_import.cmake)

project(pi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(src)
add_subdirectory(docs)
