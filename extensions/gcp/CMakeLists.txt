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

if (NOT (ENABLE_ALL OR ENABLE_GCP))
  return()
endif()

include(GoogleCloudCpp)
include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
file(GLOB SOURCES "*.cpp" "controllerservices/*.cpp" "processors/*.cpp")

add_library(minifi-gcp SHARED ${SOURCES})

target_link_libraries(minifi-gcp ${LIBMINIFI} google-cloud-cpp::storage)
target_include_directories(minifi-gcp PUBLIC ${google-cloud-cpp_INCLUDE_DIRS})

register_extension(minifi-gcp "GCP EXTENSIONS" GCP-EXTENSIONS "This enables Google Cloud Platform support" "extensions/gcp/tests")

register_extension_linter(minifi-gcp-extensions-linter)
