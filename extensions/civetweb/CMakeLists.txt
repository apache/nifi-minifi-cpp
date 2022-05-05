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

if (DISABLE_CIVET)
  return()
endif()

include(BundledCivetWeb)
use_bundled_civetweb(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/civetweb/dummy")

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(${CMAKE_SOURCE_DIR}/libminifi/include
                    ${CMAKE_SOURCE_DIR}/libminifi/include/core
                    ${CMAKE_SOURCE_DIR}/thirdparty/
                    ./include)

file(GLOB SOURCES  "processors/*.cpp" "protocols/*.cpp")

add_library(minifi-civet-extensions SHARED ${SOURCES})
target_link_libraries(minifi-civet-extensions ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-civet-extensions CIVETWEB::c-library CIVETWEB::civetweb-cpp)

register_extension(minifi-civet-extensions CIVETWEB CIVETWEB "This enables ListenHTTP" "extensions/civetweb/tests")
register_extension_linter(minifi-civet-extensions-linter)
