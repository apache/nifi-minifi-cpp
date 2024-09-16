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

cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0135 NEW) # policy to set the timestamps of extracted contents to the time of extraction

include_directories(../libminifi/include)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    include_directories(../libminifi/opsys/win)
else()
    include_directories(../libminifi/opsys/posix)
endif()


include(CppVersion)
set_cpp_version()

set(MINIFIEXE_SOURCES MiNiFiMain.cpp MainHelper.cpp MiNiFiWindowsService.cpp AgentDocs.cpp TableFormatter.cpp)
if(WIN32)
    set_source_files_properties("${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc" PROPERTIES GENERATED TRUE)
    list(APPEND MINIFIEXE_SOURCES "${CMAKE_BINARY_DIR}/libminifi/src/core/logging/WindowsMessageTextFile.rc")

    set(EXECUTABLE_FILE_NAME "minifi.exe")
    set(EXECUTABLE_FILE_DESCRIPTION "${PRODUCT_NAME} agent")
    configure_file(${CMAKE_SOURCE_DIR}/versioninfo.rc.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc @ONLY)
    list(APPEND MINIFIEXE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc")
endif()
add_minifi_executable(minifiexe ${MINIFIEXE_SOURCES})

if (NOT USE_SHARED_LIBS)
    if (LIBC_STATIC)
        set_target_properties(minifiexe PROPERTIES LINK_SEARCH_START_STATIC 1)
    endif(LIBC_STATIC)
endif(NOT USE_SHARED_LIBS)
target_link_libraries(minifiexe Threads::Threads)

target_link_libraries(minifiexe yaml-cpp)
if(CUSTOM_MALLOC_LIB)
    message(VERBOSE "Using custom malloc lib ${CUSTOM_MALLOC_LIB} for minifiexe")
    target_link_libraries(minifiexe ${CUSTOM_MALLOC_LIB})
endif()

if (WIN32)
    include_directories(SYSTEM "../thirdparty/Simple-Windows-Posix-Semaphore")
    target_link_libraries(minifiexe semaphore)
endif()

get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
include(ArgParse)
target_link_libraries(minifiexe spdlog libsodium gsl-lite argparse core-minifi)

set_target_properties(minifiexe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(minifiexe PROPERTIES OUTPUT_NAME minifi)
set_target_properties(minifiexe PROPERTIES ENABLE_EXPORTS True)
if (WIN32)
    target_compile_definitions(minifiexe PUBLIC SERVICE_NAME="Apache NiFi MINiFi")
endif()

if (WIN32)
    add_custom_command(TARGET minifiexe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:minifiexe> ${CMAKE_BINARY_DIR}/minifi_main/)
endif()

if (NOT WIN32)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT APPLE)
        target_link_options(minifiexe PRIVATE "-Wl,--disable-new-dtags")
    endif()
    if (APPLE)
        set_target_properties(minifiexe PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(minifiexe PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
    install(TARGETS minifiexe RUNTIME DESTINATION bin COMPONENT bin)
endif()
