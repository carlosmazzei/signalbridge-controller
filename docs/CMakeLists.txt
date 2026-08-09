# Documentation is optional so firmware configuration remains usable without Doxygen.
find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    # set input and output files
    set(DOXYGEN_IN "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")
    set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target(doc_doxygen ALL
        COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
else()
    message(STATUS "Doxygen was not found; documentation target disabled")
endif()
