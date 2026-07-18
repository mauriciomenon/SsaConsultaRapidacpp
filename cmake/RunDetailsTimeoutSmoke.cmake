if(NOT DEFINED APP OR NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "APP and WORK_DIR are required")
endif()

set(screenshot "${WORK_DIR}/details.png")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/config")
file(MAKE_DIRECTORY "${WORK_DIR}/missing-database")

string(TIMESTAMP started "%s" UTC)
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
    QT_QUICK_CONTROLS_STYLE=Basic "${APP}" --db
    "${WORK_DIR}/missing-database/ssas.db" --config-dir "${WORK_DIR}/config"
    --screenshot "${screenshot}" --open-details-window
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 12)
string(TIMESTAMP finished "%s" UTC)
math(EXPR elapsed "${finished} - ${started}")

if(NOT result STREQUAL "2")
  message(
    FATAL_ERROR
      "details timeout returned '${result}', expected 2\n${output}${error}")
endif()
if(elapsed LESS 3)
  message(
    FATAL_ERROR
      "details capture failed before the retry timeout (${elapsed}s)\n${output}${error}"
  )
endif()
if(EXISTS "${screenshot}")
  message(FATAL_ERROR "details timeout unexpectedly produced ${screenshot}")
endif()

message(STATUS "details timeout returned exit code 2 after ${elapsed}s")
