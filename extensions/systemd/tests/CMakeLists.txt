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

function(add_systemd_test testfile)
    get_filename_component(TEST_TARGET "${testfile}" NAME_WE)
    add_executable(${TEST_TARGET} "${testfile}")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/systemd/")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test")
    target_link_libraries(${TEST_TARGET} minifi-systemd)
    target_link_libraries(${TEST_TARGET} minifi-standard-processors)
    createTests("${TEST_TARGET}")
    add_test(NAME ${TEST_TARGET} COMMAND "${TEST_TARGET}" WORKING_DIRECTORY "${TEST_DIR}")
    target_link_libraries(${TEST_TARGET} ${CATCH_MAIN_LIB})
endfunction()

add_systemd_test("ConsumeJournaldTest.cpp")
