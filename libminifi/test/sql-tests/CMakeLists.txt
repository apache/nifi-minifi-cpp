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

file(GLOB SQL_MOCK_SOURCES  "mocks/*.cpp")
add_library(minifi-sql-mocks STATIC ${SQL_MOCK_SOURCES})
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/sql")
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "mocks")
target_wholearchive_library(minifi-sql-mocks core-minifi)

file(GLOB SQL_TESTS  "*.cpp")
set(SQL_TEST_COUNT 0)
foreach(testfile ${SQL_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable(${testfilename} "${testfile}")

    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/catch")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/sql")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test")

    createTests("${testfilename}")
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    target_link_libraries(${testfilename} minifi-sql-mocks)
    target_link_libraries(${testfilename} minifi-sql)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} minifi-expression-language-extensions)
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})

    math(EXPR SQL_TEST_COUNT "${SQL_TEST_COUNT}+1")
endforeach()

message("-- Finished building ${SQL_TEST_COUNT} sql test file(s)...")
