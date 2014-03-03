set(file "/home/xcgoner/mnt_src/powerlore/deps/hadoop/src/hadoop-1.0.1.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "e627d9b688c4de03cba8313bd0bba148")
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
