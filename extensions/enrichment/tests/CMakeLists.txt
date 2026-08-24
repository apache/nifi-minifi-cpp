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

file(GLOB ENRICHMENT_TESTS  "*.cpp")

SET(EXTENSIONS_TEST_COUNT 0)
FOREACH(testfile ${ENRICHMENT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable(${testfilename} "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/enrichment/processors")
    createTests(${testfilename})
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-enrichment)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} libminifi-c-unittest)
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME ${testfilename} COMMAND ${testfilename} WORKING_DIRECTORY ${TEST_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "enrichment;memchecked")
ENDFOREACH()
message("-- Finished building ${EXTENSIONS_TEST_COUNT} enrichment related test file(s)...")
