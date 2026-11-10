# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(WIN32)
    include(GetWinFlexBison)
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
