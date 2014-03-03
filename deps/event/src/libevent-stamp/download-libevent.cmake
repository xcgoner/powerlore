message(STATUS "downloading...
     src='http://iweb.dl.sourceforge.net/project/levent/libevent/libevent-2.0/libevent-2.0.18-stable.tar.gz'
     dst='/home/xcgoner/mnt_src/powerlore/deps/event/src/libevent-2.0.18-stable.tar.gz'
     timeout='none'")




file(DOWNLOAD
  "http://iweb.dl.sourceforge.net/project/levent/libevent/libevent-2.0/libevent-2.0.18-stable.tar.gz"
  "/home/xcgoner/mnt_src/powerlore/deps/event/src/libevent-2.0.18-stable.tar.gz"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=aa1ce9bc0dee7b8084f6855765f2c86a
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://iweb.dl.sourceforge.net/project/levent/libevent/libevent-2.0/libevent-2.0.18-stable.tar.gz' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
