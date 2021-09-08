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
    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.2.2/sol.hpp" "${SOL2_INCLUDE_DIR}/sol/sol.hpp"
         EXPECTED_HASH SHA256=4aba3e893497591901af1bdff923b8b6725951a9df519a6003e5d0498b4cef69)

    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.2.2/config.hpp" "${SOL2_INCLUDE_DIR}/sol/config.hpp"
         EXPECTED_HASH SHA256=1a768cd8fdc8efeb993a4952b79204b79ac3d4bd69f55abcb605c5eba4e48b11)

    file(DOWNLOAD "https://github.com/ThePhD/sol2/releases/download/v3.2.2/forward.hpp" "${SOL2_INCLUDE_DIR}/sol/forward.hpp"
         EXPECTED_HASH SHA256=491c59790c242f8ec766deb35a18a5aae34772da3393c1b8f0719f5c50d01fdf)

    set(PC "${Patch_EXECUTABLE}" -p1 -i "${CMAKE_SOURCE_DIR}/thirdparty/sol2/add-missing-include.patch" "${SOL2_INCLUDE_DIR}/sol/sol.hpp")

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
