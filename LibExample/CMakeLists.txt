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

include_directories(../blocks/ ../libminifi/include  ../libminifi/include/c2  ../libminifi/include/c2/protocols/  ../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../libminifi/include/core/yaml  ../libminifi/include/core  ../thirdparty/spdlog-20170710/include ../thirdparty/concurrentqueue ../thirdparty/yaml-cpp-yaml-cpp-20171024/include ../thirdparty/civetweb-1.9.1/include ../thirdparty/)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Os")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Os")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

add_executable(transmit_flow transmit_flow.c)

# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl and rocksdb
target_link_libraries(transmit_flow capi core-minifi minifi)

if (APPLE)
	target_link_libraries (transmit_flow -Wl,-all_load minifi-http-curl)
else ()
	target_link_libraries (transmit_flow -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive)
endif ()


add_executable(generate_flow generate_flow.c)

# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl and rocksdb
target_link_libraries(generate_flow capi core-minifi minifi)

if (APPLE)
	target_link_libraries (generate_flow -Wl,-all_load minifi-http-curl)
else ()
	target_link_libraries (generate_flow -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive)
endif ()


add_executable(monitor_directory monitor_directory.c)

# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl and rocksdb
target_link_libraries(monitor_directory capi core-minifi minifi)

if (APPLE)
	target_link_libraries (monitor_directory -Wl,-all_load minifi-http-curl)
else ()
	target_link_libraries (monitor_directory -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive)
endif ()
