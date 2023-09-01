add_library(interfaces inputs.c led.c cobs.c)

target_include_directories(interfaces PUBLIC 
                            $(CMAKE_CURRENT_SOURCE_DIR)
                            ${CMAKE_SOURCE_DIR}/src/FreeRTOS
                            )

target_link_libraries(interfaces PUBLIC  
                        pico_stdlib 
                        hardware_spi
                        hardware_adc
                        FreeRTOS-Kernel 
                        FreeRTOS-Kernel-Heap1 
                    )
