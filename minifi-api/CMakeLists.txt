add_library(minifi-api INTERFACE)
target_include_directories(minifi-api INTERFACE include)
target_link_libraries(minifi-api INTERFACE gsl-lite)

add_library(minifi-c-api INTERFACE)
target_include_directories(minifi-c-api INTERFACE include/minifi-c)
if (APPLE)
  target_link_options(minifi-c-api INTERFACE -undefined dynamic_lookup)
endif()

