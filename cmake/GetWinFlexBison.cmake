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

if(MINIFI_WINFLEXBISON_SOURCE STREQUAL "CONAN")
    message("Using Conan to install winflexbison")
    find_program(WIN_FLEX_EXECUTABLE NAMES win_flex REQUIRED)
    find_program(WIN_BISON_EXECUTABLE NAMES win_bison REQUIRED)

    set(BISON_EXECUTABLE "${WIN_BISON_EXECUTABLE}" CACHE PATH "bison executable")
    set(FLEX_EXECUTABLE "${WIN_FLEX_EXECUTABLE}" CACHE PATH "flex executable")

    get_filename_component(WINFLEXBISON_BIN_DIR "${WIN_FLEX_EXECUTABLE}" DIRECTORY)
    get_filename_component(WINFLEXBISON_ROOT_DIR "${WINFLEXBISON_BIN_DIR}" DIRECTORY)
    include_directories("${WINFLEXBISON_ROOT_DIR}/include")

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/winflexbison")
elseif(MINIFI_WINFLEXBISON_SOURCE STREQUAL "BUILD")
    message("Using CMake to build winflexbison from source")
    include(FetchContent)

    set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/winflexbison/minimum_cmake_version.patch")

    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

    FetchContent_Declare(
        winflexbison
        URL "https://github.com/lexxmark/winflexbison/archive/refs/tags/v2.5.25.tar.gz"
        URL_HASH "SHA256=8e1b71e037b524ba3f576babb0cf59182061df1f19cd86112f085a882560f60b"
        PATCH_COMMAND "${PC}"
        SYSTEM
    )
    FetchContent_GetProperties("winflexbison")

    if(NOT winflexbison_POPULATED)
        FetchContent_Populate("winflexbison")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release .
            WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
        )

        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config Release
            WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()

    set(BISON_EXECUTABLE "${winflexbison_SOURCE_DIR}/bin/Release/win_bison.exe" CACHE PATH "bison executable")
    set(FLEX_EXECUTABLE "${winflexbison_SOURCE_DIR}/bin/Release/win_flex.exe" CACHE PATH "flex executable")

    include_directories(${winflexbison_SOURCE_DIR}/flex/src/)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/winflexbison")
endif()
