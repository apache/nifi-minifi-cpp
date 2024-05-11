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
if(USE_CONAN_PACKAGER)
    message("Using Conan Packager to manage installing prebuilt PcapPlusPlus external lib")
    include(${CMAKE_BINARY_DIR}/pcapplusplus-config.cmake)

    set(PCAPPLUSPLUS_HEADER_DIR "${pcapplusplus_INCLUDE_DIRS}" )
    set(PCAPPLUSPLUS_HEADER_DIR PCAPPLUSPLUS_HEADER_DIR PARENT_SCOPE)

    set(PCAPPLUSPLUS_LIBRARIES "${pcapplusplus_LIBRARIES}")

elseif(USE_CMAKE_FETCH_CONTENT)
    message("Using CMAKE's FetchContent to manage source building PcapPlusPlus external lib")

    find_package(PCAP REQUIRED)

    include(FetchContent)
    
    FetchContent_Declare(pcapplusplus
        URL      https://github.com/seladb/PcapPlusPlus/archive/refs/tags/v22.05.tar.gz
        URL_HASH SHA256=5f299c4503bf5d3c29f82b8d876a19be7dea29c2aadcb52f2f3b394846c21da9
        )
    FetchContent_MakeAvailable(pcapplusplus)
    
    include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
    
    set(PCAPPLUSPLUS_TP_BASE_DIR "${pcapplusplus_SOURCE_DIR}/")
    set(PCAPPLUSPLUS_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcap++" )
    set(PCAPPLUSPLUS_BASE_DIR PCAPPLUSPLUS_BASE_DIR PARENT_SCOPE)
    set(PCAPPLUSPLUS_HEADER_DIR "${PCAPPLUSPLUS_BASE_DIR}/Dist/header" )
    set(PCAPPLUSPLUS_HEADER_DIR PCAPPLUSPLUS_HEADER_DIR PARENT_SCOPE)
    
    file(COPY ${PCAPPLUSPLUS_TP_BASE_DIR} DESTINATION ${PCAPPLUSPLUS_BASE_DIR})
    
    if (WIN32)
        execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-windows-visual-studio.bat WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
    elseif (APPLE)
        execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-mac_os_x.sh  WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
    else ()
        execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-linux.sh --default WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
    endif ()
    
    set(PCAPPLUSPLUS_LIB_DIR "${PCAPPLUSPLUS_BASE_DIR}/Dist")
    
    add_custom_target(
        pcappp
        COMMAND make libs
        BYPRODUCTS ${PCAPPLUSPLUS_LIB_DIR}/libPcap++.a ${PCAPPLUSPLUS_LIB_DIR}/libPacket++.a ${PCAPPLUSPLUS_LIB_DIR}/libCommon++.a
        WORKING_DIRECTORY ${PCAPPLUSPLUS_BASE_DIR}
    )
    
    list(APPEND PCAPPLUSPLUS_LIBRARIES ${PCAPPLUSPLUS_LIB_DIR}/libPcap++.a ${PCAPPLUSPLUS_LIB_DIR}/libPacket++.a ${PCAPPLUSPLUS_LIB_DIR}/libCommon++.a)
endif()
