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

if (NOT ((ENABLE_ALL OR ENABLE_SFTP) AND NOT DISABLE_CURL))
  return()
endif()

include(BundledLibSSH2)
use_bundled_libssh2(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/libssh2/dummy")

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(client processors)

file(GLOB SOURCES  "*.cpp" "client/*.cpp" "processors/*.cpp")

add_library(minifi-sftp SHARED ${SOURCES})

target_link_libraries(minifi-sftp ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-sftp CURL::libcurl libssh2 RapidJSON)

register_extension(minifi-sftp "SFTP EXTENSIONS" SFTP "This enables SFTP support" "extensions/sftp/tests")
register_extension_linter(minifi-sftp-linter)
