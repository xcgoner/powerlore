set(file "/home/xcgoner/mnt_src/powerlore/deps/tcmalloc/src/gperftools-2.0.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "13f6e8961bc6a26749783137995786b6")
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
