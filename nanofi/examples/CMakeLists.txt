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

include_directories(/include)

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

if (WIN32)
    set(LINK_FLAGS "/WHOLEARCHIVE")
    set(LINK_END_FLAGS "")
elseif (APPLE)
    set(LINK_FLAGS "-Wl,-all_load")
    set(LINK_END_FLAGS "")
else ()
    set(LINK_FLAGS "-Wl,--whole-archive")
    set(LINK_END_FLAGS "")
endif ()

if (NOT WIN32)

add_executable(generate_flow generate_flow.c)

add_executable(terminate_handler terminate_handler.c)

target_link_libraries(generate_flow nanofi ${CMAKE_THREAD_LIBS_INIT} ${LINK_FLAGS} minifi-http-curl ${LINK_END_FLAGS})

target_link_libraries(terminate_handler nanofi ${CMAKE_THREAD_LIBS_INIT} )

add_executable(transmit_flow transmit_flow.c)

target_link_libraries(transmit_flow nanofi ${CMAKE_THREAD_LIBS_INIT}  ${LINK_FLAGS} minifi-http-curl ${LINK_END_FLAGS})

add_executable(transmit_payload transmit_payload.c)

target_link_libraries(transmit_payload nanofi ${CMAKE_THREAD_LIBS_INIT}  ${LINK_FLAGS} minifi-http-curl ${LINK_END_FLAGS})

add_executable(monitor_directory monitor_directory.c)

target_link_libraries(monitor_directory nanofi ${CMAKE_THREAD_LIBS_INIT}  ${LINK_FLAGS} minifi-http-curl ${LINK_END_FLAGS})

endif()
