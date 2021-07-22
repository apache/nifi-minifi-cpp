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

find_package(PCAP REQUIRED)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(/usr/include/netinet)

set(PCAPPLUSPLUS_TP_BASE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/pcap++/")
set(PCAPPLUSPLUS_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcap++" )
set(PCAPPLUSPLUS_BASE_DIR PCAPPLUSPLUS_BASE_DIR PARENT_SCOPE)
set(PCAPPLUSPLUS_HEADER_DIR "${PCAPPLUSPLUS_BASE_DIR}/Dist/header" )
set(PCAPPLUSPLUS_HEADER_DIR PCAPPLUSPLUS_HEADER_DIR PARENT_SCOPE)

include_directories(${PCAPPLUSPLUS_HEADER_DIR})

file(COPY ${PCAPPLUSPLUS_TP_BASE_DIR} DESTINATION ${PCAPPLUSPLUS_BASE_DIR})

if (WIN32)
	execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-windows-visual-studio.bat WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
elseif (APPLE)
	execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-mac_os_x.sh  WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
else ()
	execute_process(COMMAND  ${PCAPPLUSPLUS_BASE_DIR}/configure-linux.sh --default WORKING_DIRECTORY  ${PCAPPLUSPLUS_BASE_DIR})
endif ()

add_custom_target(
   pcappp
   COMMAND make libs
   WORKING_DIRECTORY ${PCAPPLUSPLUS_BASE_DIR}
)

set(PCAPPLUSPLUS_LIB_DIR "${PCAPPLUSPLUS_BASE_DIR}/Dist")

file(GLOB SOURCES  "*.cpp")

add_library(minifi-pcap SHARED ${SOURCES})

add_dependencies(minifi-pcap pcappp)

target_link_libraries (minifi-pcap ${PCAPPLUSPLUS_LIB_DIR}/libPcap++.a ${PCAPPLUSPLUS_LIB_DIR}/libPacket++.a ${PCAPPLUSPLUS_LIB_DIR}/libCommon++.a ${PCAP_LIBRARIES})
if (APPLE)
	target_link_libraries(minifi-pcap "-framework CoreFoundation")
	target_link_libraries(minifi-pcap "-framework SystemConfiguration")
	set_target_properties(minifi-pcap PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
endif ()

target_link_libraries(minifi-pcap ${LIBMINIFI} Threads::Threads)

SET (PCAP-EXTENSION minifi-pcap PARENT_SCOPE)
register_extension(minifi-pcap)
register_extension_linter(minifi-pcap-linter)
