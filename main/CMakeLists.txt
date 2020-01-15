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

include_directories(../libminifi/include)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	include_directories(../libminifi/opsys/win)
else()
	include_directories(../libminifi/opsys/posix)
endif()


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

set(MINIFIEXE_SOURCES MiNiFiMain.cpp MainHelper.cpp MiNiFiWindowsService.cpp AgentDocs.cpp)
if(WIN32)
	set_source_files_properties("${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc" PROPERTIES GENERATED TRUE)
	list(APPEND MINIFIEXE_SOURCES "${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc")
endif()
add_executable(minifiexe ${MINIFIEXE_SOURCES})

if (NOT USE_SHARED_LIBS)
	if (LIBC_STATIC)
		set_target_properties(minifiexe PROPERTIES LINK_SEARCH_START_STATIC 1)
	endif(LIBC_STATIC)
endif(NOT USE_SHARED_LIBS)
#if(THREADS_HAVE_PTHREAD_ARG)
#  target_compile_options(PUBLIC minifiexe "-pthread")
#endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifiexe "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_wholearchive_library(minifiexe core-minifi)
target_wholearchive_library(minifiexe minifi)

target_link_libraries(minifiexe yaml-cpp)
if(NOT WIN32 AND ENABLE_JNI AND NOT DISABLE_JEMALLOC)
	target_link_libraries(minifiexe JeMalloc::JeMalloc)
endif()

if (WIN32)
	include_directories("../thirdparty/Simple-Windows-Posix-Semaphore")
  	target_link_libraries(minifiexe semaphore)
endif()

if (NOT USE_SHARED_LIBS)
	if (LIBC_STATIC)
		target_link_libraries(minifiexe -static-libgcc -static-libstdc++)
		set_target_properties(minifiexe PROPERTIES LINK_SEARCH_END_STATIC 1)
	endif(LIBC_STATIC)
endif(NOT USE_SHARED_LIBS)

get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
foreach(EXTENSION ${extensions})
	message("Linking MiNiFiMain against ${EXTENSION}")
	target_wholearchive_library(minifiexe ${EXTENSION})
endforeach()

set_target_properties(minifiexe PROPERTIES OUTPUT_NAME minifi)

if (NOT WIN32)
add_custom_command(TARGET minifiexe POST_BUILD
           COMMAND cat ${CMAKE_BINARY_DIR}/all.log)
else()
#add_custom_command(TARGET minifiexe POST_BUILD
#           COMMAND type ${CMAKE_BINARY_DIR}/all.log)
endif()
