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

if (NOT stduuid_FOUND)
    set(UUID_SYSTEM_GENERATOR "ON")
    set(stduuid_FOUND "YES" CACHE STRING "" FORCE)
    set(stduuid_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/stduuid/" CACHE STRING "" FORCE)
    if(NOT EXISTS "${stduuid_INCLUDE_DIR}/stduuid/uuid.hpp")
        file(DOWNLOAD "https://github.com/mariusbancila/stduuid/releases/download/v1.2.3/uuid.h" "${stduuid_INCLUDE_DIR}/stduuid/uuid.hpp"
                EXPECTED_HASH SHA256=8b329afa7e099e632c2e992e02ddb9fc4627c772dfd5fd42b069752ea0f8ec7f)
    endif()
endif()

if(NOT TARGET stduuid::stduuid)
    add_library(stduuid::stduuid INTERFACE IMPORTED)
    set_target_properties(stduuid::stduuid PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${stduuid_INCLUDE_DIR}")
endif()
