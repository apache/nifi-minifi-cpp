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

# Controller Tests are not supported on windows
if (WIN32)
    return()
endif()

file(GLOB CONTROLLER_TESTS  "*.cpp")

file(GLOB CONTROLLER_SOURCES  "${CMAKE_SOURCE_DIR}/controller/*.cpp")
list(REMOVE_ITEM CONTROLLER_SOURCES "${CMAKE_SOURCE_DIR}/controller/MiNiFiController.cpp")

set(CONTROLLER_TEST_COUNT 0)
foreach(testfile ${CONTROLLER_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable(${testfilename} "${testfile}" "${CONTROLLER_SOURCES}")

    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/controller")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test")

    createTests(${testfilename})

    target_link_libraries(${testfilename} Catch2WithMain ${LIBMINIFI} ${TEST_BASE_LIB})
    add_test(NAME ${testfilename} COMMAND ${testfilename} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    math(EXPR CONTROLLER_TEST_COUNT "${CONTROLLER_TEST_COUNT}+1")

    # Copy test resources
    add_custom_command(
        TARGET "${testfilename}"
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/controller/tests/resources"
                "$<TARGET_FILE_DIR:${testfilename}>/resources"
        )
endforeach()

message("-- Finished building ${CONTROLLER_TEST_COUNT} controller test file(s)...")
