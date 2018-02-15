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

project(nifi-libcore-minifi)
set(PROJECT_NAME "nifi-libcore-minifi")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 4)
set(PROJECT_VERSION_PATCH 0)

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF (IOS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-abi-version=2 -fobjc-arc -std=gnu++11 -stdlib=libc++ -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-abi-version=2 -fobjc-arc -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
ELSE ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENSSL_SUPPORT -DYAML_SUPPORT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOPENSSL_SUPPORT -DYAML_SUPPORT")
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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-reorder")

include_directories(../thirdparty/spdlog-20170710/include)
include_directories(../thirdparty/yaml-cpp-yaml-cpp-20171024/include)

include_directories(../thirdparty/rapidjson-1.1.0/include)
include_directories(../thirdparty/concurrentqueue/)
include_directories(include)

file(GLOB SOURCES  "src/sitetosite/*.cpp"  "src/core/logging/*.cpp"  "src/core/state/*.cpp" "src/c2/protocols/*.cpp" "src/c2/*.cpp" "src/io/*.cpp" "src/io/tls/*.cpp" "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp"  "src/provenance/*.cpp" "src/utils/*.cpp" "src/*.cpp")

file(GLOB PROCESSOR_SOURCES  "src/processors/*.cpp" )

file(GLOB CAPI_SOURCES  "src/capi/*.cpp" )

file(GLOB SPD_SOURCES "../thirdparty/spdlog-20170710/include/spdlog/*")

# Workaround the limitations of having a
# header only library
add_library(spdlog STATIC ${SPD_SOURCES})
add_library(core-minifi STATIC ${SOURCES})
target_link_libraries(core-minifi ${CMAKE_DL_LIBS} ${UUID_LIBRARIES} yaml-cpp)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

target_link_libraries(core-minifi minifi-expression-language-extensions)
target_link_libraries (core-minifi ${ZLIB_LIBRARIES})

# Include OpenSSL
find_package (OpenSSL REQUIRED)
if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
	target_link_libraries (core-minifi ${OPENSSL_LIBRARIES})
else ()
    message( FATAL_ERROR "OpenSSL was not found. Please install OpenSSL" )
endif (OPENSSL_FOUND)

add_library(minifi STATIC ${PROCESSOR_SOURCES})
add_library(capi STATIC ${CAPI_SOURCES})

target_link_libraries(minifi core-minifi)


SET (LIBMINIFI core-minifi PARENT_SCOPE)
