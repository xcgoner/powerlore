message(STATUS "downloading...
     src='http://graphlab.org/deps/hadoop-1.0.1.tar.gz'
     dst='/home/xcgoner/mnt_src/powerlore/deps/hadoop/src/hadoop-1.0.1.tar.gz'
     timeout='none'")




file(DOWNLOAD
  "http://graphlab.org/deps/hadoop-1.0.1.tar.gz"
  "/home/xcgoner/mnt_src/powerlore/deps/hadoop/src/hadoop-1.0.1.tar.gz"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=e627d9b688c4de03cba8313bd0bba148
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://graphlab.org/deps/hadoop-1.0.1.tar.gz' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
