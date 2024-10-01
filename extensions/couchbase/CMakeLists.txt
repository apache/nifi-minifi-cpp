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

if (NOT (ENABLE_ALL OR ENABLE_COUCHBASE))
    return()
endif()

include(Couchbase)
include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "controllerservices/*.cpp" "processors/*.cpp")

add_minifi_library(minifi-couchbase SHARED ${SOURCES})

target_include_directories(minifi-couchbase PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}" "controllerservices" "processors" ${COUCHBASE_INCLUDE_DIR})
target_link_libraries(minifi-couchbase ${LIBMINIFI} couchbase_cxx_client_static hdr_histogram_static snappy llhttp::llhttp)

register_extension(minifi-couchbase "COUCHBASE EXTENSIONS" COUCHBASE-EXTENSIONS "This enables Couchbase support" "extensions/couchbase/tests")

register_extension_linter(minifi-couchbase-extensions-linter)
