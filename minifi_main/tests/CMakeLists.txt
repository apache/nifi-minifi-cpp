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

file(GLOB MINIFI_MAIN_UNIT_TESTS  "*.cpp")
SET(MINIFI_MAIN_UNIT_TEST_COUNT 0)
FOREACH(TEST_FILE ${MINIFI_MAIN_UNIT_TESTS})
    get_filename_component(TEST_TARGET "${TEST_FILE}" NAME_WE)
    add_minifi_executable("${TEST_TARGET}" "${TEST_FILE}" "../TableFormatter.cpp")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE
            "${CMAKE_SOURCE_DIR}/libminifi/test"
            "${CMAKE_SOURCE_DIR}/minifi_main")
    target_link_libraries(${TEST_TARGET} core-minifi Catch2WithMain)
    createTests("${TEST_TARGET}")
    add_test(NAME ${TEST_TARGET} COMMAND "${TEST_TARGET}" WORKING_DIRECTORY "${TEST_DIR}")
    MATH(EXPR MINIFI_MAIN_UNIT_TEST_COUNT "${MINIFI_MAIN_UNIT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${MINIFI_MAIN_UNIT_TEST_COUNT} MiNiFi main unit test file(s)...")
copyTestResources(${CMAKE_SOURCE_DIR}/libminifi/test/resources/certs ${CMAKE_BINARY_DIR}/bin/resources)
