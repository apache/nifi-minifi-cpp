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

file(GLOB EXECUTESCRIPT_LUA_TESTS "*.cpp")

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_LUA_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/lua")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${SOL2_INCLUDE_DIR}")
    target_link_libraries(${testfilename} minifi-lua-script-extension minifi-script-extension)
    target_link_libraries(${testfilename} Catch2WithMain)
    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

copyTestResources(${CMAKE_CURRENT_SOURCE_DIR}/test_lua_scripts/ ${CMAKE_BINARY_DIR}/bin/resources/test_lua_scripts/)
message("-- Finished building ${EXTENSIONS_TEST_COUNT} tests for minifi-lua-script-extension...")
