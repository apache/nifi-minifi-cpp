# Licensed to the Apache Software Foundation (ASF) under one
#
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

cmake_minimum_required (VERSION 2.6)

project(nifi-libminifi)
set(PROJECT_NAME "nifi-libminifi")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)


#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
# https://cmake.org/cmake/help/v3.0/policy/CMP0048.html#policy:CMP0048
IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF (IOS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-abi-version=2 -fobjc-arc -std=gnu++11 -stdlib=libc++ -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-abi-version=2 -fobjc-arc -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
ELSE ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLEVELDB_SUPPORT -DOPENSSL_SUPPORT -DYAML_SUPPORT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLEVELDB_SUPPORT -DOPENSSL_SUPPORT -DYAML_SUPPORT")
ENDIF()

include_directories(../include)
include_directories(../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include)
include_directories(include)

file(GLOB SOURCES "src/*.cpp")
file(GLOB SPD_SOURCES "../include/spdlog/*")

# Workaround the limitations of having a
# header only library
add_library(spdlog STATIC ${SPD_SOURCES})
add_library(minifi STATIC ${SOURCES})

# Include LevelDB
if (NOT IOS)
find_package (Leveldb REQUIRED)
if (LEVELDB_FOUND)
	include_directories(${LEVELDB_INCLUDE_DIRS})
	target_link_libraries (minifi ${LEVELDB_LIBRARIES})
else ()
    message( FATAL_ERROR "LevelDB was not found. Please install LevelDB" )
endif (LEVELDB_FOUND)
endif ()

# Include OpenSSL 
if (NOT IOS)
find_package (OpenSSL REQUIRED)
if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
	target_link_libraries (minifi ${OPENSSL_LIBRARIES})
else ()
    message( FATAL_ERROR "OpenSSL was not found. Please install OpenSSL" )
endif (OPENSSL_FOUND)
endif ()
