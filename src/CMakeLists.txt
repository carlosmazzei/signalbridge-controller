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
    ${CMAKE_CURRENT_SOURCE_DIR}/error_management.c
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
    -Os                                 # Optimize for size
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
    $<$<CONFIG:Debug>:"LINKER:--print-memory-usage">   # Show memory usage (debug only)
    
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

# Doxygen: Run memory analysis script after build
add_custom_command(
    TARGET pi_controller POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/memory_analysis.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running memory_analysis.sh after build"
)
