if(NOT DEFINED APP OR NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "APP and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/root/data")
file(MAKE_DIRECTORY "${WORK_DIR}/root/config")
# Preferences written by a newer version: the app must refuse them and say why
# in its log, since a GUI build or app bundle has no console.
file(WRITE "${WORK_DIR}/root/config/ssa_cpp_preferences.json"
     "{\"schema_version\": 999}")

set(QT_ENVIRONMENT QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic)
if(WIN32)
  list(APPEND QT_ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env ${QT_ENVIRONMENT} "${APP}" --project-root
    "${WORK_DIR}/root" --db "${WORK_DIR}/root/data/ssas.db" --config-dir
    "${WORK_DIR}/root/config" --smoke-exit-ms 3000
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 12)

if(NOT result EQUAL 2)
  message(
    FATAL_ERROR "expected startup exit code 2, got ${result}\n${output}${error}"
  )
endif()
set(LOG_FILE "${WORK_DIR}/root/config/logs/ssa.log")
if(NOT EXISTS "${LOG_FILE}")
  message(FATAL_ERROR "startup failure left no log file at ${LOG_FILE}")
endif()
file(READ "${LOG_FILE}" log_output)
if(NOT log_output MATCHES "Startup failed: unsupported future schema_version")
  message(FATAL_ERROR "startup failure is missing from the log:\n${log_output}")
endif()
message(STATUS "STARTUP_FAILURE_SMOKE logged")
