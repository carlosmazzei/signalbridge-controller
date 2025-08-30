cmake_minimum_required(VERSION 3.14)

# Enable compilation database for tools
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only load Pico SDK for embedded builds
if(NOT HOST_BUILD)
    set(FAMILY rp2040)
    set(BOARD pico_sdk)

    # Pull in SDK (must be before project)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk/")
    include(lib/pico-sdk/pico_sdk_init.cmake)

    if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.0")
        message(FATAL_ERROR "Require minimum SDK version 1.5.0")
    endif()
endif()

# Check Pico SDK version and pull in FreeRTOS (only for embedded builds)
if(NOT HOST_BUILD)
    # Pull in FreeRTOS
    set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel/")
    include(FreeRTOS_Kernel_import.cmake)
endif()

project(pi_controller C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize Pico SDK (only for embedded builds)
if(NOT HOST_BUILD)
    pico_sdk_init()
endif()

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(docs)

# Add tests (only for host builds)
if(HOST_BUILD)
    include(CTest)
    add_subdirectory(test)
    
    # Add individual test targets for direct execution
    get_property(TEST_TARGETS DIRECTORY test PROPERTY BUILDSYSTEM_TARGETS)
    foreach(test_target ${TEST_TARGETS})
        if(test_target MATCHES "^test_" AND NOT test_target STREQUAL "test_all")
            get_target_property(target_type ${test_target} TYPE)
            if(target_type STREQUAL "EXECUTABLE")
                add_custom_target(run_${test_target}
                    COMMAND $<TARGET_FILE:${test_target}>
                    DEPENDS ${test_target}
                    COMMENT "Running ${test_target}"
                )
            endif()
        endif()
    endforeach()
endif()