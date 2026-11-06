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

add_subdirectory(common)

file(GLOB SOURCES
        src/*.cpp
        src/core/*.cpp
        src/core/extension/*.cpp
        src/io/*.cpp
        src/http/*.cpp
        src/utils/*.cpp
        src/utils/crypto/*.cpp
        src/utils/crypto/ciphers/*.cpp
        src/utils/crypto/property_encryption/*.cpp
        src/utils/net/*.cpp
        src/utils/file/*.cpp)

add_minifi_library(minifi-core-framework STATIC ${SOURCES})
target_compile_definitions(minifi-core-framework PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi")
target_include_directories(minifi-core-framework PUBLIC include)
target_link_libraries(minifi-core-framework PUBLIC minifi-api minifi-core-framework-common ZLIB::ZLIB concurrentqueue::concurrentqueue rapidjson spdlog Threads::Threads gsl-lite::gsl-lite libsodium::libsodium range-v3::range-v3 date::date date::tz asio::asio magic_enum::magic_enum OpenSSL::Crypto OpenSSL::SSL CURL::libcurl)
if(NOT WIN32)
    target_link_libraries(minifi-core-framework PUBLIC OSSP::libuuid++)
endif()
