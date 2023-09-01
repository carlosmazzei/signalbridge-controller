add_executable(pi_controller)

# Source
target_sources(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/hooks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
        )

# Include
target_include_directories(pi_controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS
        )

#pico_enable_stdio_usb(pi_controller 1)
#pico_enable_stdio_uart(pi_controller 0)

pico_add_extra_outputs(pi_controller) 

add_subdirectory(lib)

target_link_libraries(  pi_controller 
                        interfaces
                        pico_stdlib 
                        hardware_spi
                        hardware_adc
                        pico_unique_id 
                        tinyusb_device 
                        tinyusb_board 
                        FreeRTOS-Kernel 
                        FreeRTOS-Kernel-Heap1 
                    )

