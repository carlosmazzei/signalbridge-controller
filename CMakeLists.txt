cmake_minimum_required(VERSION 3.14)

# Enable compilation database for tools
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FAMILY rp2040)
set(BOARD pico_sdk)

# Pull in SDK (must be before project)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk/")
include(lib/pico-sdk/pico_sdk_init.cmake)

# Check Pico SDK version
if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.0")
    message(FATAL_ERROR "Require minimum SDK version 1.5.0")
endif()

# Pull in FreeRTOS
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel/")
include(FreeRTOS_Kernel_import.cmake)

project(pi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(docs)