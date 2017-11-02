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

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-all-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--export-symbols")


find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(
        el-parser
        ${CMAKE_CURRENT_SOURCE_DIR}/Parser.yy
        ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
)

flex_target(
        el-scanner
        ${CMAKE_CURRENT_SOURCE_DIR}/Scanner.ll
        ${CMAKE_CURRENT_BINARY_DIR}/Scanner.cpp
)

add_flex_bison_dependency(el-scanner el-parser)

include_directories(${CMAKE_SOURCE_DIR}/libminifi/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/
                    ${CMAKE_CURRENT_BINARY_DIR}/
                    ./include)

file(GLOB SOURCES "*.cpp")

add_library(minifi-expression-language-extensions STATIC ${SOURCES} ${BISON_el-parser_OUTPUTS} ${FLEX_el-scanner_OUTPUTS})
set_property(TARGET minifi-expression-language-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-expression-language-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-expression-language-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

find_package(UUID REQUIRED)
target_link_libraries(minifi-expression-language-extensions ${LIBMINIFI})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

target_link_libraries(minifi-expression-language-extensions ${CMAKE_DL_LIBS})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-expression-language-extensions ${ZLIB_LIBRARIES})

if (WIN32)
    set_target_properties(minifi-expression-language-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-expression-language-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-expression-language-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

SET (EXPRESSION-LANGUAGE-EXTENSIONS minifi-expression-language-extensions PARENT_SCOPE)

register_extension(minifi-expression-language-extensions)

add_custom_target(expression-language-extension-linter
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/
            ${CMAKE_CURRENT_LIST_DIR}/include --
            ${CMAKE_CURRENT_LIST_DIR}/)

register_extension_linter(expression-language-extension-linter)