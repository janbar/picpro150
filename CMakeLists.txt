cmake_minimum_required(VERSION 3.8.2...3.20)
project(picpro VERSION 1.2.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(serialport)

set (SRC_FILES
  main.cpp
  k150.cpp
  chipinfo.cpp
  hexdata.cpp
)

add_executable(picpro ${SRC_FILES})

target_link_libraries(picpro serialport)
