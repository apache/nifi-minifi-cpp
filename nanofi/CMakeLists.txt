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

include(CheckCXXCompilerFlag)
if (WIN32)
  if ((MSVC_VERSION GREATER "1900") OR (MSVC_VERSION EQUAL "1900"))
	    CHECK_CXX_COMPILER_FLAG("/std:c++14" _cpp_latest_flag_supported)
	    if (_cpp_latest_flag_supported)
	        add_compile_options("/std:c++14")
	    endif()
	endif()
else()

CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

endif()

add_library(nanofi STATIC ${NANOFI_SOURCES})

target_link_libraries(nanofi spdlog ut)
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
