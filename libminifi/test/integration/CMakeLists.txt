# Licensed to the Apache Software Foundation (ASF) under one
#
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

GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

SET(TESTS_WITH_LIBARCHIVE_DEPENDENCY C2DebugBundleTest)
SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}")
    createIntegrationTests("${testfilename}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} Catch2WithMain Threads::Threads)
    target_compile_definitions(${testfilename} PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    if(NOT ${testfilename} IN_LIST TESTS_WITH_LIBARCHIVE_DEPENDENCY)
        add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    endif()
    MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()

FOREACH(testfilename ${TESTS_WITH_LIBARCHIVE_DEPENDENCY})
    if (ENABLE_LIBARCHIVE)
        add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    endif()
ENDFOREACH()

target_link_libraries(OnScheduleErrorHandlingTests minifi-test-processors)

message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")
