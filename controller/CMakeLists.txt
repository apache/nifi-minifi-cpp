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

include_directories(../minifi_main/ ../libminifi/include  ../libminifi/include/c2  ../libminifi/include/c2/protocols/  ../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../libminifi/include/core/yaml  ../libminifi/include/core)

if(WIN32)
    include_directories(../libminifi/opsys/win)
else()
    include_directories(../libminifi/opsys/posix)
endif()

include(CppVersion)
set_cpp_version()

set(MINIFI_CONTROLLER_SOURCES MiNiFiController.cpp Controller.cpp ../minifi_main/MainHelper.cpp)
if (WIN32)
    set(EXECUTABLE_FILE_NAME "minifi-controller.exe")
    set(EXECUTABLE_FILE_DESCRIPTION "${PRODUCT_NAME} controller")
    configure_file(${CMAKE_SOURCE_DIR}/versioninfo.rc.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc @ONLY)
    list(APPEND MINIFI_CONTROLLER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc")
endif()

add_minifi_executable(minifi-controller ${MINIFI_CONTROLLER_SOURCES})
include(ArgParse)
target_link_libraries(minifi-controller ${LIBMINIFI} argparse Threads::Threads)

set_target_properties(minifi-controller PROPERTIES
    OUTPUT_NAME minifi-controller
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (NOT WIN32)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT APPLE)
        target_link_options(minifi-controller PRIVATE "-Wl,--disable-new-dtags")
    endif()
    if (APPLE)
        set_target_properties(minifi-controller PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(minifi-controller PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
endif()

install(TARGETS minifi-controller RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin)
