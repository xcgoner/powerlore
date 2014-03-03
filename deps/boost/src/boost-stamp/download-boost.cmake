message(STATUS "downloading...
     src='http://sourceforge.net/projects/boost/files/boost/1.53.0/boost_1_53_0.tar.gz'
     dst='/home/xcgoner/mnt_src/powerlore/deps/boost/src/boost_1_53_0.tar.gz'
     timeout='none'")




file(DOWNLOAD
  "http://sourceforge.net/projects/boost/files/boost/1.53.0/boost_1_53_0.tar.gz"
  "/home/xcgoner/mnt_src/powerlore/deps/boost/src/boost_1_53_0.tar.gz"
  SHOW_PROGRESS
  EXPECTED_HASH;MD5=57a9e2047c0f511c4dfcf00eb5eb2fbb
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://sourceforge.net/projects/boost/files/boost/1.53.0/boost_1_53_0.tar.gz' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
