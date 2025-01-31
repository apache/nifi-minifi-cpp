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

file(GLOB ENCRYPT_CONFIG_FILES  "*.cpp")
if (WIN32)
    set(EXECUTABLE_FILE_NAME "encrypt-config.exe")
    set(EXECUTABLE_FILE_DESCRIPTION "${PRODUCT_NAME} encrypt-config tool")
    configure_file(${CMAKE_SOURCE_DIR}/versioninfo.rc.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc @ONLY)
    list(APPEND ENCRYPT_CONFIG_FILES "${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc")
endif()
add_minifi_executable(encrypt-config "${ENCRYPT_CONFIG_FILES}")
target_include_directories(encrypt-config PRIVATE ../libminifi/include)
include(ArgParse)
target_link_libraries(encrypt-config libsodium argparse core-minifi)
set_target_properties(encrypt-config PROPERTIES OUTPUT_NAME encrypt-config)
set_target_properties(encrypt-config PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (NOT WIN32)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT APPLE)
        target_link_options(encrypt-config PRIVATE "-Wl,--disable-new-dtags")
    endif()
    if (APPLE)
        set_target_properties(encrypt-config PROPERTIES INSTALL_RPATH "@loader_path")
    else()
        set_target_properties(encrypt-config PROPERTIES INSTALL_RPATH "$ORIGIN")
    endif()
endif()
install(TARGETS encrypt-config RUNTIME DESTINATION bin COMPONENT bin)
