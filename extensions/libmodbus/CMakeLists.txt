include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include(ExternalProject)

ExternalProject_Add(
  libmodbus_external
  PREFIX ${LIBMODBUS_ROOT}
  GIT_REPOSITORY "https://github.com/stephane/libmodbus.git"
  UPDATE_COMMAND ""
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/extensions/libmodbus/ConfigureAndBuild.bat ${CMAKE_BINARY_DIR}/extensions/libmodbus/libmodbus_external-prefix/src/libmodbus_external/src/win32
  DOWNLOAD_DIR ${LIBMODBUS_ROOT}
  LOG_DOWNLOAD ON
)
