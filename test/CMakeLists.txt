# Test-only CMakeLists.txt for host builds with FreeRTOS simulator
cmake_minimum_required(VERSION 3.14)

project(signalbridge_tests C)
set(CMAKE_C_STANDARD 11)

# Options for coverage
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Find CMocka package
find_library(CMOCKA_LIBRARIES cmocka REQUIRED)
find_path(CMOCKA_INCLUDE_DIRS cmocka.h)

if(NOT CMOCKA_LIBRARIES)
    message(FATAL_ERROR "CMocka library not found. Please install libcmocka-dev")
endif()

if(NOT CMOCKA_INCLUDE_DIRS)
    message(FATAL_ERROR "CMocka headers not found. Please install libcmocka-dev")
endif()

# FreeRTOS is now handled by the core library - no need to duplicate here

# Test target configuration with CMocka best practices
function(add_unit_test test_name)
    # Parse additional arguments for wrap functions
    set(options)
    set(oneValueArgs)
    set(multiValueArgs WRAP_FUNCTIONS)
    cmake_parse_arguments(UNIT_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create test executable (FreeRTOS sources now in core library)
    add_executable(${test_name} ${UNIT_TEST_UNPARSED_ARGUMENTS})
    
    # Link with the core library (which includes FreeRTOS POSIX) and CMocka
    target_link_libraries(${test_name} 
        ${CMOCKA_LIBRARIES} 
        signalbridge_core  # Includes FreeRTOS POSIX and pthread
    )
    
    # Include directories (minimal since core library handles most)
    target_include_directories(${test_name} PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../include  # Project headers
        ${CMOCKA_INCLUDE_DIRS}
    )
    
    # Add wrap functions if specified
    if(UNIT_TEST_WRAP_FUNCTIONS)
        foreach(func ${UNIT_TEST_WRAP_FUNCTIONS})
            target_link_options(${test_name} PRIVATE -Wl,--wrap,${func})
        endforeach()
    endif()
    
    # Compiler-specific options
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(${test_name} PRIVATE 
            -Wall -Wextra -Wpedantic
            $<$<BOOL:${ENABLE_COVERAGE}>:--coverage -fprofile-arcs -ftest-coverage>
        )
        target_link_options(${test_name} PRIVATE 
            $<$<BOOL:${ENABLE_COVERAGE}>:--coverage>
        )
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${test_name} PRIVATE 
            -Wall -Wextra -Wpedantic
            $<$<BOOL:${ENABLE_COVERAGE}>:-fprofile-instr-generate -fcoverage-mapping>
        )
        target_link_options(${test_name} PRIVATE 
            $<$<BOOL:${ENABLE_COVERAGE}>:-fprofile-instr-generate -fcoverage-mapping>
        )
    endif()
    
    # Register test with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        LABELS "unit"
        TIMEOUT 30
    )
endfunction()

# Build or reuse the core library
# When this test project is configured standalone (from test/), we need to
# add the core library subdirectory. When included from the root project,
# the target already exists and we should not re-add it.
if(NOT TARGET signalbridge_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_BINARY_DIR}/src)
endif()

# Enable testing
enable_testing()

# Add unit tests
add_subdirectory(unit)

# Custom targets for convenience
add_custom_target(test_all
    COMMAND ctest --output-on-failure --verbose
    COMMENT "Running all unit tests"
    DEPENDS test_cobs test_error_management test_outputs test_tm1639 test_inputs
)

# Coverage target (only available if coverage is enabled)
if(ENABLE_COVERAGE)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ctest --output-on-failure
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '*/lib/*' --output-file coverage.info
            COMMAND ${GENHTML_PATH} coverage.info --output-directory coverage
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in ${CMAKE_BINARY_DIR}/coverage/index.html"
            COMMENT "Generating coverage report"
            DEPENDS test_cobs test_error_management test_outputs test_tm1639 test_inputs
        )
    else()
        message(STATUS "lcov or genhtml not found. Coverage target disabled.")
    endif()
endif()
