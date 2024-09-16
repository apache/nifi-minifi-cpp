file(GLOB SOURCES
        src/*.cpp
        src/core/*.cpp
        src/controllers/keyvalue/*.cpp
        src/controllers/*.cpp
        src/io/*.cpp
        src/serialization/*.cpp
        src/utils/crypto/ciphers/*.cpp
        src/utils/property_encryption/*.cpp
        src/utils/crypto/*.cpp
        src/utils/file/*.cpp
        src/utils/net/*.cpp
        src/utils/tls/*.cpp
        src/utils/*.cpp)
add_library(minifi-extension-utils STATIC ${SOURCES})
target_include_directories(minifi-extension-utils PUBLIC include)
target_link_libraries(minifi-extension-utils PUBLIC minifi-core)

include(RangeV3)
include(Asio)
include(MagicEnum)
list(APPEND CORE_LIBRARIES ZLIB::ZLIB concurrentqueue RapidJSON spdlog Threads::Threads gsl-lite libsodium range-v3 expected-lite date::date date::tz asio magic_enum OpenSSL::Crypto OpenSSL::SSL CURL::libcurl RapidJSON)
if(NOT WIN32)
    list(APPEND CORE_LIBRARIES OSSP::libuuid++)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    list(APPEND CORE_LIBRARIES stdc++fs)
endif()
target_link_libraries(minifi-extension-utils PUBLIC ${CMAKE_DL_LIBS} ${CORE_LIBRARIES})
if (APPLE)
    target_link_libraries(minifi-extension-utils PUBLIC "-framework CoreFoundation -framework SystemConfiguration")
endif()
if (UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7"))
    target_link_libraries(minifi-extension-utils PUBLIC "-latomic")
endif()

target_link_libraries(minifi-extension-utils PRIVATE minifi-core)
target_link_libraries(minifi-extension-utils PUBLIC minifi-utils)