# Builds libmodbus (https://github.com/stephane/libmodbus.git) library.
# ConfigureAndBuild.bat calls `cscript configure.js` as described in `ibmodbus/src/win32/` and builds libmodbus.lib Release-x86.

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include(ExternalProject)

ExternalProject_Add(
  libmodbus_external
  PREFIX ${LIBMODBUS_ROOT}
  GIT_REPOSITORY "https://github.com/stephane/libmodbus.git"
  UPDATE_COMMAND ""
  CMAKE_COMMAND ""
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/extensions/libmodbus/ConfigureAndBuild.bat ${CMAKE_BINARY_DIR}/extensions/libmodbus/libmodbus_external-prefix/src/libmodbus_external/src/win32
  DOWNLOAD_DIR ${LIBMODBUS_ROOT}
  LOG_DOWNLOAD ON
)
