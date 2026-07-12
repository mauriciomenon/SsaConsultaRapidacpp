if(NOT DEFINED APP
   OR NOT DEFINED OPTION
   OR NOT DEFINED VALUE
   OR NOT DEFINED EXPECTED)
  message(FATAL_ERROR "APP, OPTION, VALUE and EXPECTED are required")
endif()

execute_process(
  COMMAND "${APP}" --db missing.sqlite "${OPTION}" "${VALUE}"
  RESULT_VARIABLE RESULT
  OUTPUT_VARIABLE STDOUT
  ERROR_VARIABLE STDERR)

if(RESULT EQUAL 0)
  message(FATAL_ERROR "ssa_mem_stress unexpectedly accepted ${OPTION}=${VALUE}")
endif()

set(OUTPUT "${STDOUT}${STDERR}")
if(NOT OUTPUT MATCHES "${EXPECTED}")
  message(FATAL_ERROR "unexpected output: ${OUTPUT}")
endif()
