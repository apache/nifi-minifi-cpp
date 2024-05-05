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
    message("Using Conan Packager to manage installing prebuilt magic_enum external lib")
    include(${CMAKE_BINARY_DIR}/magic_enum-config.cmake)
    target_include_directories(magic_enum::magic_enum INTERFACE ${magic_enum_INCLUDE_DIRS} ${magic_enum_INCLUDE_DIRS}/magic_enum)
elseif(USE_CMAKE_FETCH_CONTENT)
    message("Using CMAKE's FetchContent to manage source building magic_enum external lib")

    include(FetchContent)

    FetchContent_Declare(magic_enum
            URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.3.tar.gz
            URL_HASH SHA256=3cadd6a05f1bffc5141e5e731c46b2b73c2dbff025e723c8abaa659e0a24f072)
    
    FetchContent_MakeAvailable(magic_enum)

    add_library(magic_enum::magic_enum ALIAS magic_enum)
endif()
