add_library(minifi-api-common INTERFACE)
target_include_directories(minifi-api-common INTERFACE common/include)
target_link_libraries(minifi-api-common INTERFACE gsl-lite)

add_library(minifi-api INTERFACE)
target_include_directories(minifi-api INTERFACE include)
target_link_libraries(minifi-api INTERFACE minifi-api-common)

add_library(minifi-c-api INTERFACE)
target_include_directories(minifi-c-api INTERFACE include/minifi-c)
if (APPLE)
  target_link_options(minifi-c-api INTERFACE -undefined dynamic_lookup)
endif()

if (WIN32)
  set(IMPORT_LIB_FILE ${CMAKE_CURRENT_BINARY_DIR}/minifi-c-api.lib)
  add_custom_target(minifi_c_api_import_lib
    COMMAND lib.exe /def:${CMAKE_CURRENT_SOURCE_DIR}/minifi-c-api.def /out:${IMPORT_LIB_FILE} /machine:x64
    BYPRODUCTS ${IMPORT_LIB_FILE}
  )

  add_dependencies(minifi-c-api minifi_c_api_import_lib)
  target_link_libraries(minifi-c-api INTERFACE ${IMPORT_LIB_FILE})
endif()
