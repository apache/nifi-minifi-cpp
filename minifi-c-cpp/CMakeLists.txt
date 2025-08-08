file(GLOB SOURCES "src/*.cpp" "src/core/*.cpp" "src/core/logging/*.cpp" "src/io/*.cpp" "src/utils/*.cpp")

add_library(minifi-c-cpp STATIC ${SOURCES})
target_include_directories(minifi-c-cpp PUBLIC include)
target_link_libraries(minifi-c-cpp PUBLIC minifi-c-api)
