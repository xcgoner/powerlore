set(file "/home/xcgoner/mnt_src/powerlore/deps/boost/src/boost_1_53_0.tar.gz")
message(STATUS "verifying file...
     file='${file}'")
set(expect_value "57a9e2047c0f511c4dfcf00eb5eb2fbb")
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
