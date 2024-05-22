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

function(get_curl SOURCE_DIR BINARY_DIR)
    if(MINIFI_USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt libcurl external lib")
        include(${CMAKE_BINARY_DIR}/CURLConfig.cmake)
    else()
        message("Using CMAKE's ExternalProject_Add to manage source building libcurl external lib")
        include(BundledLibcURL)
        use_bundled_curl(${SOURCE_DIR} ${BINARY_DIR})
    endif()
endfunction(get_curl SOURCE_DIR BINARY_DIR)
