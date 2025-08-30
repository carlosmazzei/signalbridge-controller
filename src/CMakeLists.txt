# Create single static library for all modules (always available for testing)
add_library(signalbridge_core STATIC
    cobs.c
    error_management.c
    inputs.c
    outputs.c
    tm1639.c
)

target_include_directories(signalbridge_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Add platform-specific dependencies to core library
if(PICO_PLATFORM)
    # Pico target: Use RP2040 port and Pico SDK
    target_link_libraries(signalbridge_core PUBLIC
        pico_stdlib
        hardware_gpio
        hardware_watchdog
        hardware_spi
        hardware_pwm
        hardware_clocks
        hardware_pio
        hardware_adc
        FreeRTOS-Kernel  # This uses RP2040 port automatically
    )
else()
    # Host/test builds: Use FreeRTOS POSIX port and mock headers
    set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib/FreeRTOS-Kernel)
    set(FREERTOS_POSIX_PORT_PATH ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix)
    
    # FreeRTOS POSIX port sources
    set(FREERTOS_POSIX_SOURCES
        ${FREERTOS_KERNEL_PATH}/tasks.c
        ${FREERTOS_KERNEL_PATH}/list.c
        ${FREERTOS_KERNEL_PATH}/queue.c
        ${FREERTOS_KERNEL_PATH}/timers.c
        ${FREERTOS_KERNEL_PATH}/event_groups.c
        ${FREERTOS_KERNEL_PATH}/stream_buffer.c
        ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_3.c
        ${FREERTOS_POSIX_PORT_PATH}/port.c
        ${FREERTOS_POSIX_PORT_PATH}/utils/wait_for_event.c
    )
    
    # Add FreeRTOS sources to core library
    target_sources(signalbridge_core PRIVATE ${FREERTOS_POSIX_SOURCES})
    
    # FreeRTOS POSIX include directories
    # Put real FreeRTOS headers before mocks so FreeRTOS APIs resolve correctly
    target_include_directories(signalbridge_core BEFORE PUBLIC 
        ${FREERTOS_KERNEL_PATH}/include      # FreeRTOS headers
        ${FREERTOS_POSIX_PORT_PATH}          # POSIX port headers
        ${CMAKE_CURRENT_SOURCE_DIR}/../test/unit/mock_headers  # Mock Pico SDK headers
        ${CMAKE_CURRENT_SOURCE_DIR}/../test  # For FreeRTOSConfig.h
    )
    
    # Link pthread for POSIX port
    target_link_libraries(signalbridge_core PUBLIC pthread)
endif()

# Only build executable for Pico platform
if(NOT PICO_PLATFORM)
    return()
endif()

add_executable(pi_controller)

# Main application source files
target_sources(pi_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hooks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
)

# Link the core library
target_link_libraries(pi_controller PRIVATE
    signalbridge_core
)

target_compile_definitions(pi_controller PRIVATE
    PICO_USE_FASTEST_SUPPORTED_CLOCK=1
    PICO_STACK_SIZE=0x1000 # 4KB main stack
    PICO_CORE1_STACK_SIZE=0x1000 # 4KB Core 1 stack
    PICO_HEAP_SZIE=0x20000
    PICO_USE_STACK_GUARDS=1
    PICO_STACK_GUARDS=1
)

# COMPILER DEBUG OPTIONS - Enable debugging features
target_compile_options(pi_controller PRIVATE
    # Debug information
    $<$<CONFIG:Debug>:-g3>              # Full debug info
    $<$<CONFIG:Debug>:-DDEBUG>          # Enable debug macros
    
    # Stack monitoring
    -fstack-usage                       # Generate .su files for stack analysis
    -Wstack-usage=3584                  # Warn if function uses >3.5KB stack (leave 0.5KB margin)
    
    # Memory optimization
    -ffunction-sections                 # Allow unused function removal
    -fdata-sections                     # Allow unused data removal
    
    # Additional warnings
    -Wall                               # Enable most warnings
    -Wextra                             # Enable extra warnings
    -Wshadow                            # Warn about shadowed variables
    
    # Stack and memory warnings
    -Wstack-protector                   # Stack protection warnings
    -fstack-protector-strong            # Enable stack protection
)

# LINKER OPTIONS
target_link_options(pi_controller PRIVATE
    # Memory optimization
    "LINKER:--gc-sections"              # Remove unused sections
    "LINKER:--cref"                     # Generate cross-reference
    
    # Debug information
    $<$<CONFIG:Debug>:LINKER:--print-memory-usage>   # Show memory usage (debug only)
    
    # Map file generation
    "LINKER:-Map=${CMAKE_CURRENT_BINARY_DIR}/pi_controller.map"
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
target_link_libraries(pi_controller PRIVATE
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

# Run memory analysis script after build
add_custom_target(memory_analysis ALL
    DEPENDS pi_controller
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/memory_analysis.sh
            ${CMAKE_CURRENT_BINARY_DIR}/pi_controller.elf
            ${CMAKE_CURRENT_BINARY_DIR}/memory_analysis_report.txt
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running memory_analysis.sh after build"
)
