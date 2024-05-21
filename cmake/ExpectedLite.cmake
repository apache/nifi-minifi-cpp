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
if(USE_CONAN_PACKAGER)
    message("Using Conan Packager to manage installing prebuilt expected-lite external lib")
    include(${CMAKE_BINARY_DIR}/expected-lite-config.cmake)
elseif(USE_CMAKE_FETCH_CONTENT)
    message("Using CMAKE's FetchContent to manage source building expected-lite external lib")

    include(FetchContent)

    FetchContent_Declare(expected-lite
        URL      https://github.com/martinmoene/expected-lite/archive/refs/tags/v0.6.0.tar.gz
        URL_HASH SHA256=90478ff7345100bf7539b12ea2c5ff04a7b6290bc5c280f02b473d5c65165342
    )
    FetchContent_MakeAvailable(expected-lite)
endif()
