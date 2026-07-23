if(NOT DEFINED APP
   OR NOT DEFINED WORK_DIR
   OR NOT DEFINED WIDTH
   OR NOT DEFINED HEIGHT
   OR NOT DEFINED MODE)
  message(FATAL_ERROR "APP, WORK_DIR, WIDTH, HEIGHT and MODE are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/root/data")
file(MAKE_DIRECTORY "${WORK_DIR}/root/config")
file(MAKE_DIRECTORY "${WORK_DIR}/root/docs_entrada")
set(SCREENSHOT "${WORK_DIR}/smoke-${WIDTH}x${HEIGHT}.png")
set(ARGUMENTS
    --project-root
    "${WORK_DIR}/root"
    --db
    "${WORK_DIR}/root/data/ssas.db"
    --config-dir
    "${WORK_DIR}/root/config"
    --screenshot
    "${SCREENSHOT}"
    --smoke-window-width
    "${WIDTH}"
    --smoke-window-height
    "${HEIGHT}")

if(MODE STREQUAL "popup")
  list(APPEND ARGUMENTS --smoke-advanced-popup)
  set(EXPECTED "QML_POPUP_SMOKE.*\"success\":true")
elseif(MODE STREQUAL "layout")
  list(APPEND ARGUMENTS --smoke-layout)
  set(EXPECTED "QML_LAYOUT_SMOKE.*\"success\":true")
else()
  message(FATAL_ERROR "unsupported smoke MODE '${MODE}'")
endif()

set(QT_ENVIRONMENT QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic)
if(WIN32)
  list(APPEND QT_ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${QT_ENVIRONMENT} "${APP}" ${ARGUMENTS}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 12)

set(COMBINED "${output}${error}")
set(LOG_FILE "${WORK_DIR}/root/config/logs/ssa.log")
if(EXISTS "${LOG_FILE}")
  file(READ "${LOG_FILE}" log_output)
  string(APPEND COMBINED "${log_output}")
endif()
if(NOT result EQUAL 0)
  message(FATAL_ERROR "${MODE} smoke failed with ${result}\n${COMBINED}")
endif()
if(COMBINED MATCHES "Both point size and pixel size set\\. Using pixel size\\.")
  message(
    FATAL_ERROR "${MODE} smoke emitted conflicting font sizes\n${COMBINED}")
endif()
if(NOT COMBINED MATCHES "${EXPECTED}")
  message(FATAL_ERROR "${MODE} smoke returned no valid metrics\n${COMBINED}")
endif()
if(NOT EXISTS "${SCREENSHOT}")
  message(FATAL_ERROR "${MODE} smoke did not create ${SCREENSHOT}")
endif()
file(SIZE "${SCREENSHOT}" SCREENSHOT_SIZE)
if(SCREENSHOT_SIZE LESS 1024)
  message(
    FATAL_ERROR
      "${MODE} smoke screenshot is incomplete (${SCREENSHOT_SIZE} bytes)")
endif()

message(STATUS "${COMBINED}")
message(STATUS "${MODE} smoke wrote ${SCREENSHOT_SIZE} bytes to ${SCREENSHOT}")
