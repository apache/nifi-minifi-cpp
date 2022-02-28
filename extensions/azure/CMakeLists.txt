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

option(ENABLE_AZURE "Enables Azure support." OFF)

if (NOT (ENABLE_ALL OR ENABLE_AZURE))
  return()
endif()

include(BundledAzureSdkCpp)
use_bundled_libazure(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp" "storage/*.cpp" "controllerservices/*.cpp" "processors/*.cpp" "utils/*.cpp")

add_library(minifi-azure SHARED ${SOURCES})

target_compile_features(minifi-azure PUBLIC cxx_std_14)

target_include_directories(minifi-azure BEFORE PRIVATE controllerservices)
target_include_directories(minifi-azure BEFORE PRIVATE processors)
target_include_directories(minifi-azure BEFORE PRIVATE storage)
target_include_directories(minifi-azure BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/extensions/azure)

target_link_libraries(minifi-azure ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-azure CURL::libcurl LibXml2::LibXml2)
target_link_libraries(minifi-azure AZURE::azure-storage-files-datalake AZURE::azure-storage-blobs AZURE::azure-storage-common AZURE::azure-core AZURE::azure-identity)

if (WIN32)
  target_link_libraries(minifi-azure crypt32.lib bcrypt.lib)
endif()

register_extension(minifi-azure "AZURE EXTENSIONS" AZURE-EXTENSIONS "This enables Azure support" "${TEST_DIR}/azure-tests")

register_extension_linter(minifi-azure-extensions-linter)
