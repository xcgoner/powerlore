set(file "/home/xcgoner/mnt_src/powerlore/deps/json/src/libjson_7.6.0.zip")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "dcb326038bd9b710b8f717580c647833")
file(MD5 "${file}" actual_value)
if("${actual_value}" STREQUAL "${expect_value}")
  message(STATUS "verifying file... done")
else()
  message(FATAL_ERROR "error: MD5 hash of
  ${file}
does not match expected value
  expected: ${expect_value}
    actual: ${actual_value}
")
endif()
