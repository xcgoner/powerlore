message(STATUS "downloading...
     src='http://graphlab.org/deps/eigen_3.1.2.tar.bz2'
     dst='/home/xcgoner/mnt_src/powerlore/deps/eigen/src/eigen_3.1.2.tar.bz2'
     timeout='none'")




file(DOWNLOAD
  "http://graphlab.org/deps/eigen_3.1.2.tar.bz2"
  "/home/xcgoner/mnt_src/powerlore/deps/eigen/src/eigen_3.1.2.tar.bz2"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=e9c081360dde5e7dcb8eba3c8430fde2
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://graphlab.org/deps/eigen_3.1.2.tar.bz2' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
