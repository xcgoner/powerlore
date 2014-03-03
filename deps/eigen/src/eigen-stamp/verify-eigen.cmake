set(file "/home/xcgoner/mnt_src/powerlore/deps/eigen/src/eigen_3.1.2.tar.bz2")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "e9c081360dde5e7dcb8eba3c8430fde2")
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
