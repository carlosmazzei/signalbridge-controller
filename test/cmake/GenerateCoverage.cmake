cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "WORK_DIR not set")
endif()
if(NOT DEFINED COVERAGE_DIR)
  message(FATAL_ERROR "COVERAGE_DIR not set")
endif()
if(NOT DEFINED LCOV_PATH OR NOT DEFINED GENHTML_PATH)
  message(FATAL_ERROR "LCOV_PATH or GENHTML_PATH not set")
endif()

# Ensure output directory exists
file(MAKE_DIRECTORY "${COVERAGE_DIR}")

# Capture coverage data
execute_process(
  COMMAND "${LCOV_PATH}" --directory . --capture --output-file coverage.info
  WORKING_DIRECTORY "${WORK_DIR}"
  RESULT_VARIABLE _lcov_capture_rv
)
if(NOT _lcov_capture_rv EQUAL 0)
  message(FATAL_ERROR "lcov capture failed: ${_lcov_capture_rv}")
endif()

# Filter out system and vendored libs; ignore 'unused' warnings if patterns don't match
execute_process(
  COMMAND "${LCOV_PATH}" --ignore-errors unused --remove coverage.info "/usr/*" --output-file coverage.info
  WORKING_DIRECTORY "${WORK_DIR}"
)
execute_process(
  COMMAND "${LCOV_PATH}" --ignore-errors unused --remove coverage.info "*/lib/*" --output-file coverage.info
  WORKING_DIRECTORY "${WORK_DIR}"
)

# Generate HTML report
execute_process(
  COMMAND "${GENHTML_PATH}" coverage.info --output-directory "${COVERAGE_DIR}"
  WORKING_DIRECTORY "${WORK_DIR}"
  RESULT_VARIABLE _genhtml_rv
)
if(NOT _genhtml_rv EQUAL 0)
  message(FATAL_ERROR "genhtml failed: ${_genhtml_rv}")
endif()

message(STATUS "Coverage report generated in ${COVERAGE_DIR}/index.html")

