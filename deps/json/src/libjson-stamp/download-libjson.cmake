message(STATUS "downloading...
     src='http://graphlab.org/deps/libjson_7.6.0.zip'
     dst='/home/xcgoner/mnt_src/powerlore/deps/json/src/libjson_7.6.0.zip'
     timeout='none'")




file(DOWNLOAD
  "http://graphlab.org/deps/libjson_7.6.0.zip"
  "/home/xcgoner/mnt_src/powerlore/deps/json/src/libjson_7.6.0.zip"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=dcb326038bd9b710b8f717580c647833
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://graphlab.org/deps/libjson_7.6.0.zip' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
