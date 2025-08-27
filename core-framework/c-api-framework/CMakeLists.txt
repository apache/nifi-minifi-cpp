file(GLOB SOURCES src/core/*.cpp src/core/logging/*.cpp src/utils/*.cpp)

add_minifi_library(minifi-core-c-api-framework STATIC ${SOURCES})
target_include_directories(minifi-core-c-api-framework PUBLIC include)
target_link_libraries(minifi-core-c-api-framework PUBLIC minifi-core-framework-common minifi-c-api)
