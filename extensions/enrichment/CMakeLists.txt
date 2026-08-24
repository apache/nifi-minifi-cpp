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

if (NOT (ENABLE_ALL OR MINIFI_EXTENSION_ENRICHMENT))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-enrichment SHARED ${SOURCES})
target_include_directories(minifi-enrichment PUBLIC "${CMAKE_SOURCE_DIR}/extensions/enrichment")
target_include_directories(minifi-enrichment PUBLIC "${ENRICHMENT_INCLUDE_DIRS}")

include(Fetchstduuid)
target_link_libraries(minifi-enrichment minifi-cpp-extension-lib stduuid::stduuid)

register_c_api_extension(minifi-enrichment "ENRICHMENT EXTENSION" ENRICHMENT-EXTENSION "Provides Fork/Join enrichment processors" "extensions/enrichment/tests")
