set(HEADER_FILES
    cobs.h
    data_event.h
    FreeRTOSConfig.h
    hooks.h
    inputs.h
    outputs.h
    commands.h
    task_props.h
    tm1639.h
    tusb_config.h
)

# Check the existence of each header file
foreach(header ${HEADER_FILES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${header})
        message(FATAL_ERROR "Header file not found: ${header}")
    endif()
endforeach()

add_library(pico_controller_headers INTERFACE)
target_include_directories(pico_controller_headers INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FREERTOS_KERNEL_PATH}/include
        ${PICO_SDK_PATH}/src/common/pico_stdlib/include
    )

