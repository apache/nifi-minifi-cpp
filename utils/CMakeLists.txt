file(GLOB SOURCES
        src/*.cpp
        src/io/*.cpp
        src/utils/*.cpp)

add_library(minifi-utils STATIC ${SOURCES})
target_include_directories(minifi-utils PUBLIC include)
target_link_libraries(minifi-utils PUBLIC minifi-core magic_enum gsl-lite range-v3 expected-lite spdlog)
if(NOT WIN32)
    target_link_libraries(minifi-utils PUBLIC OSSP::libuuid++)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minifi-utils PUBLIC stdc++fs)
endif()