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
include_directories(../libminifi/include ../thirdparty/spdlog-20170710/include)
include_directories(../thirdparty/ut)

if(WIN32)
include_directories(../libminifi/opsys/win)
else()
include_directories(../libminifi/opsys/posix)
endif()

file(GLOB NANOFI_SOURCES  "src/api/*.cpp" "src/core/*.c*" "src/cxx/*.cpp" "src/sitetosite/*.c*")

file(GLOB NANOFI_EXAMPLES_SOURCES  "examples/*.c" )

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

if (APPLE)
	target_link_libraries (nanofi -Wl,-all_load core-minifi minifi)
elseif(NOT WIN32)
	target_link_libraries (nanofi -Wl,--whole-archive core-minifi minifi -Wl,--no-whole-archive)
else()
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:core-minifi")
	set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:minifi")
endif ()

if(WIN32)
	set_target_properties(nanofi PROPERTIES LINK_FLAGS "${WIN32_ARCHIVES}")
endif()

if (ENABLE_PYTHON AND NOT STATIC_BUILD)

add_library(nanofi-shared SHARED ${NANOFI_SOURCES})

if (APPLE)
	target_link_libraries (nanofi-shared -Wl,-all_load core-minifi-shared minifi-shared)
elseif(NOT WIN32)
	target_link_libraries (nanofi-shared -Wl,--whole-archive core-minifi-shared minifi-shared -Wl,--no-whole-archive)
else()
    set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:core-minifi-shared")
	set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:minifi-shared")
endif ()


if(WIN32)
	set_target_properties(nanofi-shared PROPERTIES LINK_FLAGS "${WIN32_ARCHIVES}")
endif()

set_property(TARGET nanofi-shared PROPERTY POSITION_INDEPENDENT_CODE ON)

endif(ENABLE_PYTHON AND NOT STATIC_BUILD)

if (NOT DISABLE_CURL)
add_subdirectory(examples)
endif()