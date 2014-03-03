set(file "/home/xcgoner/mnt_src/powerlore/deps/libbz2/src/bzip2-1.0.6.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "00b516f4704d4a7cb50a1d97e6e8e15b")
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
