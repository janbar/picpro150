cmake_minimum_required(VERSION 3.8.2...3.20)
project(picpro VERSION 1.3 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(serialport)

set (SRC_FILES
  main.cpp
  k150.cpp
  chipinfo.cpp
  hexdata.cpp
)

set(GIT_COMMAND git rev-parse --verify HEAD --short)
execute_process(
  COMMAND ${GIT_COMMAND} OUTPUT_VARIABLE BUILD_NUMBER
)
string(STRIP ${BUILD_NUMBER} BUILD_NUMBER)
set(VERSION_STRING "${CMAKE_PROJECT_VERSION}-${BUILD_NUMBER}")
message(STATUS "Build version ${VERSION_STRING}")

add_definitions(-DVERSION_STRING="${VERSION_STRING}")

add_executable(picpro ${SRC_FILES})

target_link_libraries(picpro serialport)
