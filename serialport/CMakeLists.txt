
option(SERIAL_BUILD_SHARED_LIBS "Build serialport shared library" OFF)

if (SERIAL_BUILD_SHARED_LIBS)
  add_library(serialport SHARED)
else()
  add_library(serialport STATIC)
endif()

add_library(serialport::serialport ALIAS serialport)

target_compile_features(serialport PRIVATE cxx_std_14)

target_sources(serialport
  PRIVATE
    serialport.cpp)

target_include_directories(
  serialport
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>"
)

## Installing
include(CMakePackageConfigHelpers)

install(
  TARGETS serialport
  EXPORT serialportTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)

install(
  EXPORT serialportTargets
  FILE serialportTargets.cmake
  NAMESPACE serialport::
  DESTINATION lib/cmake/serialport
)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DESTINATION include
)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/serialportConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serialport
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    serialportConfigVersion.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/serialportConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/serialportConfigVersion.cmake"
  DESTINATION lib/cmake/serialport
)
