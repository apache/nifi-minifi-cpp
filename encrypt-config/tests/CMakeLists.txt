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

file(GLOB ENCRYPT_CONFIG_TESTS  "*.cpp")

file(GLOB ENCRYPT_CONFIG_SOURCES  "${CMAKE_SOURCE_DIR}/encrypt-config/*.cpp")
list(REMOVE_ITEM ENCRYPT_CONFIG_SOURCES "${CMAKE_SOURCE_DIR}/encrypt-config/EncryptConfigMain.cpp")

set(ENCRYPT_CONFIG_TEST_COUNT 0)
foreach(testfile ${ENCRYPT_CONFIG_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable(${testfilename} "${testfile}" ${ENCRYPT_CONFIG_SOURCES})

    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/encrypt-config")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")

    createTests(${testfilename})

    target_link_libraries(${testfilename} Catch2WithMain core-minifi libminifi-unittest)
    add_test(NAME ${testfilename} COMMAND ${testfilename} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    math(EXPR ENCRYPT_CONFIG_TEST_COUNT "${ENCRYPT_CONFIG_TEST_COUNT}+1")
endforeach()

message("-- Finished building ${ENCRYPT_CONFIG_TEST_COUNT} encrypt-config test file(s)...")
