file(GLOB SOURCES
        src/*.cpp
        src/core/*.cpp
        src/io/*.cpp
        src/http/*.cpp
        src/utils/*.cpp
        src/utils/net/*.cpp)

add_library(minifi-utils STATIC ${SOURCES})
target_include_directories(minifi-utils PUBLIC include)
#target_link_libraries(minifi-utils PUBLIC minifi-core magic_enum gsl-lite range-v3 expected-lite spdlog)
target_link_libraries(minifi-utils PUBLIC minifi-core ZLIB::ZLIB concurrentqueue RapidJSON spdlog Threads::Threads gsl-lite libsodium range-v3 expected-lite date::date date::tz asio magic_enum OpenSSL::Crypto OpenSSL::SSL CURL::libcurl RapidJSON)
if(NOT WIN32)
    target_link_libraries(minifi-utils PUBLIC OSSP::libuuid++)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minifi-utils PUBLIC stdc++fs)
endif()