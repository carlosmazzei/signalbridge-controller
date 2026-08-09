set(HEADER_FILES
    cobs.h
    encoded_framer.h
    data_event.h
    FreeRTOSConfig.h
    hooks.h
    app_comm.h
    app_config.h
    app_context.h
    app_outputs.h
    app_inputs.h
    app_tasks.h
    commands.h
    task_props.h
    tm1637.h
    tm1639.h
    tusb_config.h
    error_management.h
)

# Check the existence of each header file
foreach(header ${HEADER_FILES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${header})
        message(FATAL_ERROR "Header file not found: ${header}")
    endif()
endforeach()

add_library(pico_controller_headers INTERFACE)
# Single source of truth for the project's public headers
target_include_directories(pico_controller_headers INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
