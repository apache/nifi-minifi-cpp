add_library(minifi-api INTERFACE)
target_include_directories(minifi-api INTERFACE include)
target_link_libraries(minifi-api INTERFACE gsl-lite)
