cmake_minimum_required(VERSION 3.14)

# Compilation database: presets enable CMAKE_EXPORT_COMPILE_COMMANDS.
# Create/update a symlink (Unix) or copy (others) at the source root so
# linters and verification tools always find the active build's database.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    if(UNIX)
        add_custom_target(sync_compile_commands ALL
            COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/compile_commands.json
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                    ${CMAKE_BINARY_DIR}/compile_commands.json
                    ${CMAKE_SOURCE_DIR}/compile_commands.json
            COMMENT "Symlink compile_commands.json to source root"
            BYPRODUCTS ${CMAKE_SOURCE_DIR}/compile_commands.json
        )
    else()
        add_custom_target(sync_compile_commands ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_BINARY_DIR}/compile_commands.json
                    ${CMAKE_SOURCE_DIR}/compile_commands.json
            COMMENT "Copy compile_commands.json to source root"
            BYPRODUCTS ${CMAKE_SOURCE_DIR}/compile_commands.json
        )
    endif()
endif()

# Options
option(SIGNALBRIDGE_BUILD_DOCS "Build Doxygen docs" ON)

# Only load Pico SDK for embedded builds
if(NOT HOST_BUILD)
    set(FAMILY rp2040)
    set(BOARD pico_sdk)

    # Pull in SDK (must be before project)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk/")
    include(lib/pico-sdk/pico_sdk_init.cmake)

    # Check Pico SDK version and pull in FreeRTOS (only for embedded builds)
    if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.0")
        message(FATAL_ERROR "Require minimum SDK version 1.5.0")
    endif()

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
if(SIGNALBRIDGE_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# Add tests via standard CTest flow (host builds only)
include(CTest)
if(HOST_BUILD AND BUILD_TESTING)
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
