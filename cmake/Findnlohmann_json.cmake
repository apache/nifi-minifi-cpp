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

if (NOT nlohmann_json_FOUND)
    set(nlohmann_json_FOUND "YES" CACHE STRING "" FORCE)
    set(nlohmann_json_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/nlohmann/" CACHE STRING "" FORCE)
    if(NOT EXISTS "${nlohmann_json_INCLUDE_DIR}/nlohmann/json.hpp")
        file(DOWNLOAD "https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp" "${nlohmann_json_INCLUDE_DIR}/nlohmann/json.hpp"
                EXPECTED_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63)
    endif()
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${nlohmann_json_INCLUDE_DIR}")
endif()
