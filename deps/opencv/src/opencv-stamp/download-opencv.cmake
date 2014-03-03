message(STATUS "downloading...
     src='http://superb-sea2.dl.sourceforge.net/project/opencvlibrary/opencv-unix/2.4.0/OpenCV-2.4.0.tar.bz2'
     dst='/home/xcgoner/mnt_src/powerlore/deps/opencv/src/OpenCV-2.4.0.tar.bz2'
     timeout='none'")




file(DOWNLOAD
  "http://superb-sea2.dl.sourceforge.net/project/opencvlibrary/opencv-unix/2.4.0/OpenCV-2.4.0.tar.bz2"
  "/home/xcgoner/mnt_src/powerlore/deps/opencv/src/OpenCV-2.4.0.tar.bz2"
  SHOW_PROGRESS
  # no EXPECTED_HASH
  # no TIMEOUT
  STATUS status
  LOG log)

list(GET status 0 status_code)
list(GET status 1 status_string)

if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "error: downloading 'http://superb-sea2.dl.sourceforge.net/project/opencvlibrary/opencv-unix/2.4.0/OpenCV-2.4.0.tar.bz2' failed
  status_code: ${status_code}
  status_string: ${status_string}
  log: ${log}
")
endif()

message(STATUS "downloading... done")
