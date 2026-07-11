if(NOT DEFINED PROBE)
  message(FATAL_ERROR "PROBE is required")
endif()

foreach(mode IN ITEMS eager virtual)
  set(samples)
  foreach(run RANGE 1 3)
    execute_process(
      COMMAND "${PROBE}" --mode "${mode}"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE output
      ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "${mode} RSS probe failed: ${output}${error}")
    endif()
    string(REGEX MATCH "rss=([0-9]+)" match "${output}${error}")
    if(NOT CMAKE_MATCH_1)
      message(
        FATAL_ERROR "${mode} RSS probe returned no metric: ${output}${error}")
    endif()
    list(APPEND samples "${CMAKE_MATCH_1}")
  endforeach()
  list(
    SORT samples
    COMPARE NATURAL
    ORDER ASCENDING)
  list(GET samples 1 "${mode}_median")
endforeach()

math(EXPR virtual_doubled "${virtual_median} * 2")
message(
  STATUS
    "QML_POPUP_RSS eager_median=${eager_median} virtual_median=${virtual_median}"
)
if(virtual_doubled GREATER eager_median)
  message(
    FATAL_ERROR
      "virtual popup RSS must be at most 50% of eager control: ${virtual_median} > ${eager_median}/2"
  )
endif()
