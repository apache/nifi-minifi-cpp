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

if(WIN32)
    include(FetchContent)

    set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    FetchContent_Declare(
        winflexbison
        URL "https://github.com/lexxmark/winflexbison/archive/refs/tags/v2.5.25.tar.gz"
        URL_HASH "SHA256=8e1b71e037b524ba3f576babb0cf59182061df1f19cd86112f085a882560f60b"
        SYSTEM
    )
    FetchContent_GetProperties("winflexbison")

    if(NOT winflexbison_POPULATED)
        FetchContent_Populate("winflexbison")
        execute_process(
        COMMAND ${CMAKE_COMMAND} .
        WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE bisonbuild
        ERROR_VARIABLE bisonbuildE
        )

        execute_process(
        COMMAND ${CMAKE_COMMAND} --build . --config RelWithDebInfo
        WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE bisonbuild
        ERROR_VARIABLE bisonbuildE
        )

        file(COPY ${winflexbison_SOURCE_DIR}/bison/Data DESTINATION ${winflexbison_SOURCE_DIR}/bison/RelWithDebInfo/)
    endif()

    set(BISON_EXECUTABLE "${winflexbison_SOURCE_DIR}/bison/RelWithDebInfo/win_bison.exe" CACHE PATH "bison executable")
    set(FLEX_EXECUTABLE "${winflexbison_SOURCE_DIR}/flex/RelWithDebInfo/win_flex.exe" CACHE PATH "flex executable")

    include_directories(${winflexbison_SOURCE_DIR}/flex/src/)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/winflexbison")
endif()

# On macOS brew installed bison and flex are preferred
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
    execute_process(
            COMMAND brew --prefix bison
            RESULT_VARIABLE BREW_BISON
            OUTPUT_VARIABLE BREW_BISON_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_BISON EQUAL 0 AND EXISTS "${BREW_BISON_PREFIX}")
        message(STATUS "Found Bison keg installed by Homebrew at ${BREW_BISON_PREFIX}")
        set(BISON_EXECUTABLE "${BREW_BISON_PREFIX}/bin/bison")
    endif()

    execute_process(
            COMMAND brew --prefix flex
            RESULT_VARIABLE BREW_FLEX
            OUTPUT_VARIABLE BREW_FLEX_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_FLEX EQUAL 0 AND EXISTS "${BREW_FLEX_PREFIX}")
        message(STATUS "Found Flex keg installed by Homebrew at ${BREW_FLEX_PREFIX}")
        set(FLEX_EXECUTABLE "${BREW_FLEX_PREFIX}/bin/flex")
        set(BREW_FLEX_INCLUDE "${BREW_FLEX_PREFIX}/include")
    endif()
endif()

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(
    el-parser
    ${CMAKE_SOURCE_DIR}/libminifi/include/expression-language/Parser.yy
    ${CMAKE_BINARY_DIR}/el-generated/Parser.cpp
)

flex_target(
    el-scanner
    ${CMAKE_SOURCE_DIR}/libminifi/include/expression-language/Scanner.ll
    ${CMAKE_BINARY_DIR}/el-generated/Scanner.cpp
    COMPILE_FLAGS --c++
)

set(EL_GENERATED_INCLUDE_DIR ${CMAKE_BINARY_DIR}/el-generated)
file(MAKE_DIRECTORY ${EL_GENERATED_INCLUDE_DIR})

add_flex_bison_dependency(el-scanner el-parser)

if (NOT WIN32)
    set_source_files_properties(${BISON_el-parser_OUTPUTS} PROPERTIES COMPILE_FLAGS -Wno-error)
    set_source_files_properties(${FLEX_el-scanner_OUTPUTS} PROPERTIES COMPILE_FLAGS -Wno-error)
else()
    set_source_files_properties(${BISON_el-parser_OUTPUTS} PROPERTIES COMPILE_FLAGS /wd4244)
    set_source_files_properties(${FLEX_el-scanner_OUTPUTS} PROPERTIES COMPILE_FLAGS /wd4244)
endif()
