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

