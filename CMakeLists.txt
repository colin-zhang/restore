cmake_minimum_required(VERSION 2.8)

set(PRJ logger_test)
project(${PRJ})

file(GLOB SOURCES
  logger_test.cpp
  pcap.cc
  file_reader.cpp
  access_cmdline.cpp
  rte.cpp
  gziphelper.cpp
  logger.cpp
  util.cpp
)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -g"
)
add_definitions(-mssse3)
add_definitions(-msse4)

add_definitions(-DNDEBUG)


add_executable(${PRJ} ${SOURCES})

target_link_libraries(${PRJ} pthread dl m z)