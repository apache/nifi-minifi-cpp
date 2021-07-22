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

cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0096 NEW) # policy to preserve the leading zeros in PROJECT_VERSION_{MAJOR,MINOR,PATCH,TWEAK}

include_directories(include)
include_directories(../libminifi/include)

if(WIN32)
include_directories(../libminifi/opsys/win)
else()
include_directories(../libminifi/opsys/posix)
endif()

file(GLOB NANOFI_SOURCES "src/api/*.c*" "src/core/*.c*" "src/cxx/*.cpp" "src/sitetosite/*.c*")

if(WIN32)
list(REMOVE_ITEM NANOFI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/api/ecu.c ${CMAKE_CURRENT_SOURCE_DIR}/src/core/file_utils.c ${CMAKE_CURRENT_SOURCE_DIR}/src/core/flowfiles.c)
endif()

file(GLOB NANOFI_ECU_SOURCES "ecu/*.c")

include(CppVersion)
set_cpp_version()

add_library(nanofi STATIC ${NANOFI_SOURCES})

target_link_libraries(nanofi spdlog ut core-minifi minifi-standard-processors)

if (NOT DISABLE_CURL)
add_subdirectory(examples)
add_subdirectory(ecu)
endif()
