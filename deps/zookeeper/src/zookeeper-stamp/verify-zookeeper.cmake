set(file "/home/xcgoner/mnt_src/powerlore/deps/zookeeper/src/zookeeper-3.4.5.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "f64fef86c0bf2e5e0484d19425b22dcb")
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
