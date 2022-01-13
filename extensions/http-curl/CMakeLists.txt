#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

if (DISABLE_CURL OR DISABLE_CIVET)
    return()
endif()

message("minifi-http-curl will depend on curl-external")

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(protocols client processors sitetosite)

file(GLOB SOURCES  "*.cpp" "protocols/*.cpp" "client/*.cpp" "processors/*.cpp" "sitetosite/*.cpp")

add_library(minifi-http-curl SHARED ${SOURCES})

target_link_libraries(minifi-http-curl ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-http-curl CURL::libcurl RapidJSON)

if (APPLE)
    target_link_libraries(minifi-http-curl "-framework CoreFoundation -framework SystemConfiguration")
endif()

register_extension(minifi-http-curl "HTTP CURL" HTTP-CURL "This enables RESTProtocol, InvokeHTTP, and the HTTPClient for Site to Site" "extensions/http-curl/tests/")
register_extension_linter(minifi-http-curl-linter)
