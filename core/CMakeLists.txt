add_library(minifi-core INTERFACE)
target_include_directories(minifi-core INTERFACE include)
target_link_libraries(minifi-core INTERFACE gsl-lite)
