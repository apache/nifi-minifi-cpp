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

file(GLOB PCAP_TESTS  "*.cpp")

SET(PCAP_INT_TEST_COUNT 0)
FOREACH(testfile ${PCAP_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/pcap/")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_BINARY_DIR}/extensions/pcap/pcap++/Dist/header/")
    createIntegrationTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2::Catch2WithMain)
    target_link_libraries(${testfilename} minifi-pcap)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_compile_definitions(${testfilename} PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    MATH(EXPR PCAP_INT_TEST_COUNT "${PCAP_INT_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${PCAP_INT_TEST_COUNT} libPCAP test file(s)...")
