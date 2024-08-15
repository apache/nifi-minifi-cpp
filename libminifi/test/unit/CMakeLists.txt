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

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")

if (NOT WIN32)
    list(REMOVE_ITEM UNIT_TESTS WindowsCertStoreLocationTests.cpp)
endif()

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}")
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    target_compile_definitions("${testfilename}" PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")
