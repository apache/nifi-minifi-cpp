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

if(NOT (ENABLE_ALL OR ENABLE_PCAP))
    return()
endif()

include(PcapPlusPlus)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-pcap SHARED ${SOURCES})
target_include_directories(minifi-pcap PRIVATE ${PCAPPLUSPLUS_HEADER_DIR})

target_link_libraries(minifi-pcap ${PCAPPLUSPLUS_LIBRARIES} ${PCAP_LIBRARIES})

if (APPLE)
    target_link_libraries(minifi-pcap "-framework CoreFoundation")
    target_link_libraries(minifi-pcap "-framework SystemConfiguration")
    set_target_properties(minifi-pcap PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
endif ()

target_link_libraries(minifi-pcap ${LIBMINIFI} Threads::Threads)

register_extension(minifi-pcap "PCAP EXTENSIONS" PCAP-EXTENSION "Enables libPCAP Functionality and the PacketCapture processor." "extensions/pcap/tests")
register_extension_linter(minifi-pcap-linter)
