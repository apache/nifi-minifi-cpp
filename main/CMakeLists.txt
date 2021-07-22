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

include_directories(../libminifi/include)

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	include_directories(../libminifi/opsys/win)
else()
	include_directories(../libminifi/opsys/posix)
endif()


include(CppVersion)
set_cpp_version()

set(MINIFIEXE_SOURCES MiNiFiMain.cpp MainHelper.cpp MiNiFiWindowsService.cpp AgentDocs.cpp)
if(WIN32)
	set_source_files_properties("${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc" PROPERTIES GENERATED TRUE)
	list(APPEND MINIFIEXE_SOURCES "${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc")
endif()
add_executable(minifiexe ${MINIFIEXE_SOURCES})

target_link_libraries(minifiexe Threads::Threads)

target_link_libraries(minifiexe yaml-cpp)
if(NOT WIN32 AND ENABLE_JNI AND NOT DISABLE_JEMALLOC)
	target_link_libraries(minifiexe JeMalloc::JeMalloc)
endif()

if (WIN32)
	include_directories(SYSTEM "../thirdparty/Simple-Windows-Posix-Semaphore")
  	target_link_libraries(minifiexe semaphore)
endif()

get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
target_link_libraries(minifiexe spdlog libsodium gsl-lite optional-lite ${LIBMINIFI})

set_target_properties(minifiexe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(minifiexe PROPERTIES OUTPUT_NAME minifi)
set_target_properties(minifiexe PROPERTIES ENABLE_EXPORTS True)

if (NOT WIN32)
add_custom_command(TARGET minifiexe POST_BUILD
           COMMAND cat ${CMAKE_BINARY_DIR}/all.log)
else()
#add_custom_command(TARGET minifiexe POST_BUILD
#           COMMAND type ${CMAKE_BINARY_DIR}/all.log)
endif()

if (NOT WIN32)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		target_link_options(minifiexe PRIVATE "-Wl,--disable-new-dtags")
	endif()
	set_target_properties(minifiexe PROPERTIES INSTALL_RPATH "$ORIGIN")
	install(TARGETS minifiexe RUNTIME DESTINATION bin COMPONENT bin)
endif()
