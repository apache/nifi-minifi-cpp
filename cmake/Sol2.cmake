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

# Using file(DOWNLOAD) instead of FetchContent due to this issue in cmake versions older than 3.18 https://gitlab.kitware.com/cmake/cmake/-/issues/20526

set(SOL2_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/sol2/" CACHE STRING "" FORCE)
if(NOT EXISTS "${SOL2_INCLUDE_DIR}/sol.hpp")
    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.3.0/sol.hpp" "${SOL2_INCLUDE_DIR}/sol/sol.hpp"
         EXPECTED_HASH SHA256=e095a961a5189863745e6c101124fce944af991f3d4726a1e82c5b4a885a187f)
    configure_file("${SOL2_INCLUDE_DIR}/sol/sol.hpp" "${SOL2_INCLUDE_DIR}/sol/sol.hpp" NEWLINE_STYLE LF)

    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.3.0/config.hpp" "${SOL2_INCLUDE_DIR}/sol/config.hpp"
         EXPECTED_HASH SHA256=6c283673a16f0eeb3c56f8b8d72ccf7ed3f048816dbd2584ac58564c61315f02)
    configure_file("${SOL2_INCLUDE_DIR}/sol/config.hpp" "${SOL2_INCLUDE_DIR}/sol/config.hpp" NEWLINE_STYLE LF)

    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.3.0/forward.hpp" "${SOL2_INCLUDE_DIR}/sol/forward.hpp"
         EXPECTED_HASH SHA256=8fc34d74e9b4b8baa381f5e6ab7b6f6b44114cd355c718505495943ff6b85740)
    configure_file("${SOL2_INCLUDE_DIR}/sol/forward.hpp" "${SOL2_INCLUDE_DIR}/sol/forward.hpp" NEWLINE_STYLE LF)

    # Some platform simply define LUA_COMPAT_BITLIB or LUA_COMPAT_5_2 without setting them to explicitly 1
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_SOURCE_DIR}/thirdparty/sol2/fix_bitlib_compatibility.patch" "${SOL2_INCLUDE_DIR}/sol/sol.hpp")

    execute_process(COMMAND ${PC} RESULT_VARIABLE patch_result_code)
    if(NOT patch_result_code EQUAL "0")
        message(FATAL_ERROR "Failed to patch sol.hpp")
    endif()

    add_library(sol2 INTERFACE IMPORTED)
    target_sources(sol2 INTERFACE ${SOL2_INCLUDE_DIR}/sol/sol.hpp)
    target_sources(sol2 INTERFACE ${SOL2_INCLUDE_DIR}/sol/config.hpp)
    target_sources(sol2 INTERFACE ${SOL2_INCLUDE_DIR}/sol/forward.hpp)
    target_include_directories(sol2 SYSTEM INTERFACE ${SOL2_INCLUDE_DIR})
endif()
