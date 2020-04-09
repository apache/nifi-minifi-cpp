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

cmake_minimum_required(VERSION 2.6)

IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

include_directories(include)
include_directories(../libminifi/include)

include_directories(${CMAKE_SOURCE_DIR}/nanofi/include)
include_directories(${CMAKE_SOURCE_DIR}/libminifi/include ${CMAKE_SOURCE_DIR}/thirdparty/cron ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/ut)

if(WIN32)
    include_directories(../libminifi/opsys/win)
else()
    include_directories(../libminifi/opsys/posix)
endif()

file(GLOB NANOFI_SOURCES  "src/api/*.c*" "src/core/*.c*" "src/cxx/*.cpp" "src/sitetosite/*.c*" "src/ecu_api/*.c*" "src/processors/*.c*")

if(WIN32)
    list(REMOVE_ITEM NANOFI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/api/ecu.c ${CMAKE_CURRENT_SOURCE_DIR}/src/core/flowfiles.c)
endif()

add_library(nanofi STATIC ${NANOFI_SOURCES})
add_dependencies(nanofi minifi-standard-processors)

target_link_libraries(nanofi spdlog ut libchunkio libyaml-c libcbor)
target_wholearchive_library(nanofi core-minifi)
target_wholearchive_library(nanofi minifi)
target_wholearchive_library(nanofi minifi-standard-processors)

if (ENABLE_PYTHON AND NOT STATIC_BUILD)

add_library(nanofi-shared SHARED ${NANOFI_SOURCES})
target_link_libraries(nanofi-shared spdlog ut)
target_wholearchive_library(nanofi-shared core-minifi-shared)
target_wholearchive_library(nanofi-shared minifi-shared)
target_wholearchive_library(nanofi-shared minifi-standard-processors)

add_dependencies(nanofi-shared minifi-standard-processors)

set_property(TARGET nanofi-shared PROPERTY POSITION_INDEPENDENT_CODE ON)

endif(ENABLE_PYTHON AND NOT STATIC_BUILD)

if (NOT DISABLE_CURL)
add_subdirectory(examples)
add_subdirectory(ecu)
endif()
