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

if (NOT ENABLE_CIVET)
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "processors/*.cpp" "protocols/*.cpp")

add_minifi_library(minifi-civet-extensions SHARED ${SOURCES})
target_include_directories(minifi-civet-extensions BEFORE PUBLIC
                           ${CMAKE_SOURCE_DIR}/libminifi/include
                           ${CMAKE_SOURCE_DIR}/libminifi/include/core
                           ${CMAKE_SOURCE_DIR}/thirdparty/
                           ${civetweb_SOURCE_DIR}/include
                           ./include)
target_link_libraries(minifi-civet-extensions ${LIBMINIFI} Threads::Threads civetweb::civetweb-cpp civetweb::c-library)

register_extension(minifi-civet-extensions CIVETWEB CIVETWEB "This enables ListenHTTP" "extensions/civetweb/tests")
register_extension_linter(minifi-civet-extensions-linter)
