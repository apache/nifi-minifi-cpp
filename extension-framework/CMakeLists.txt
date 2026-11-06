# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_subdirectory(cpp-extension-lib)

file(GLOB SOURCES
        src/*.cpp
        src/core/*.cpp
        src/controllers/keyvalue/*.cpp
        src/controllers/*.cpp
        src/io/*.cpp
        src/serialization/*.cpp
        src/utils/file/*.cpp
        src/utils/net/*.cpp
        src/utils/tls/*.cpp
        src/utils/*.cpp)
add_minifi_library(minifi-extension-framework STATIC ${SOURCES})
target_include_directories(minifi-extension-framework PUBLIC include)
target_link_libraries(minifi-extension-framework PUBLIC minifi-api)

list(APPEND CORE_LIBRARIES ZLIB::ZLIB concurrentqueue::concurrentqueue rapidjson spdlog Threads::Threads gsl-lite::gsl-lite range-v3::range-v3 asio::asio magic_enum::magic_enum OpenSSL::Crypto OpenSSL::SSL CURL::libcurl)
if(NOT WIN32)
    list(APPEND CORE_LIBRARIES OSSP::libuuid++)
endif()
target_link_libraries(minifi-extension-framework PUBLIC ${CMAKE_DL_LIBS} ${CORE_LIBRARIES})
if (APPLE)
    target_link_libraries(minifi-extension-framework PUBLIC "-framework CoreFoundation -framework SystemConfiguration")
endif()
if (UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7"))
    target_link_libraries(minifi-extension-framework PUBLIC "-latomic")
endif()

target_link_libraries(minifi-extension-framework PRIVATE minifi-api)
target_link_libraries(minifi-extension-framework PUBLIC minifi-core-framework)
