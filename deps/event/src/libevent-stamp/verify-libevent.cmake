set(file "/home/xcgoner/mnt_src/powerlore/deps/event/src/libevent-2.0.18-stable.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "aa1ce9bc0dee7b8084f6855765f2c86a")
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
