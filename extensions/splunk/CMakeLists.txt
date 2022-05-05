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

if (NOT (ENABLE_ALL OR ENABLE_SPLUNK))
  return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-splunk SHARED ${SOURCES})

target_include_directories(minifi-splunk PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/http-curl")
target_link_libraries(minifi-splunk ${LIBMINIFI})
target_link_libraries(minifi-splunk minifi-http-curl)

register_extension(minifi-splunk "SPLUNK EXTENSIONS" SPLUNK-EXTENSIONS "This enables Splunk support" "extensions/splunk/tests")

register_extension_linter(minifi-splunk-extensions-linter)
