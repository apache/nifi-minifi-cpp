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
set(PROJECT_VERSION_MINOR 3)
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

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

include_directories(../thirdparty/spdlog-0.13.0/include)
include_directories(../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include)
include_directories(../thirdparty/civetweb-1.9.1/include)
include_directories(../thirdparty/jsoncpp/include)
include_directories(../thirdparty/concurrentqueue/)
include_directories(include)

file(GLOB SOURCES  "src/core/logging/*.cpp"  "src/io/*.cpp" "src/io/tls/*.cpp" "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp"  "src/provenance/*.cpp" "src/processors/*.cpp" "src/utils/*.cpp" "src/*.cpp")

file(GLOB SPD_SOURCES "../thirdparty/spdlog-0.13.0/include/spdlog/*")

# Workaround the limitations of having a
# header only library
add_library(spdlog STATIC ${SPD_SOURCES})
add_library(minifi STATIC ${SOURCES})

add_dependencies(minifi jsoncpp_project)
target_link_libraries(minifi ${UUID_LIBRARIES} ${JSONCPP_LIB})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi ${ZLIB_LIBRARIES})

if (NOT IOS)
# Include Boost System
find_package(Boost COMPONENTS system REQUIRED)
find_package(CURL)
target_link_libraries(minifi ${Boost_SYSTEM_LIBRARY})

if (CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIRS})
        target_link_libraries (minifi ${CURL_LIBRARIES})
endif(CURL_FOUND)

# Include LevelDB
find_package (Leveldb REQUIRED)
if (LEVELDB_FOUND)
	include_directories(${LEVELDB_INCLUDE_DIRS})
	target_link_libraries (minifi ${LEVELDB_LIBRARIES})
else ()
    message( FATAL_ERROR "LevelDB was not found. Please install LevelDB" )
endif (LEVELDB_FOUND)

# Include OpenSSL
find_package (OpenSSL REQUIRED)
if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
	target_link_libraries (minifi ${OPENSSL_LIBRARIES})
else ()
    message( FATAL_ERROR "OpenSSL was not found. Please install OpenSSL" )
endif (OPENSSL_FOUND)
endif ()

add_subdirectory(src/utils)
