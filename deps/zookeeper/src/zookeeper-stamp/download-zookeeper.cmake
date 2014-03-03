message(STATUS "downloading...
     src='http://apache.cs.utah.edu/zookeeper/zookeeper-3.4.5/zookeeper-3.4.5.tar.gz'
     dst='/home/xcgoner/mnt_src/powerlore/deps/zookeeper/src/zookeeper-3.4.5.tar.gz'
     timeout='none'")




file(DOWNLOAD
  "http://apache.cs.utah.edu/zookeeper/zookeeper-3.4.5/zookeeper-3.4.5.tar.gz"
  "/home/xcgoner/mnt_src/powerlore/deps/zookeeper/src/zookeeper-3.4.5.tar.gz"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=f64fef86c0bf2e5e0484d19425b22dcb
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://apache.cs.utah.edu/zookeeper/zookeeper-3.4.5/zookeeper-3.4.5.tar.gz' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
