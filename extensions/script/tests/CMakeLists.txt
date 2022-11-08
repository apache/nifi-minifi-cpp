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

if (NOT DISABLE_PYTHON_SCRIPTING)
    file(GLOB EXECUTESCRIPT_PYTHON_TESTS  "TestExecuteScriptProcessorWithPythonScript.cpp" "PythonScriptEngineTests.cpp" "PythonManifestTests.cpp")
    file(GLOB EXECUTEPYTHONPROCESSOR_UNIT_TESTS  "ExecutePythonProcessorTests.cpp")
    file(GLOB PY_SOURCES  "python/*.cpp")
    find_package(PythonLibs 3.6 REQUIRED)
endif()

if (ENABLE_LUA_SCRIPTING)
    file(GLOB EXECUTESCRIPT_LUA_TESTS "TestExecuteScriptProcessorWithLuaScript.cpp" "LuaScriptEngineTests.cpp")
endif()

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_PYTHON_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script/python")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/pybind11/include")
    add_definitions(-DPYTHON_SUPPORT)
    target_link_libraries(${testfilename} minifi-script-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Copy test resources only once after the first build to be available for all test cases
    if(EXTENSIONS_TEST_COUNT EQUAL 1)
        add_custom_command(
            TARGET "${testfilename}"
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/extensions/script/tests/test_python_scripts"
                    "$<TARGET_FILE_DIR:${testfilename}>/test_python_scripts"
            )
    endif()
ENDFOREACH()

FOREACH(testfile ${EXECUTEPYTHONPROCESSOR_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")

    include_directories(${PYTHON_INCLUDE_DIR})
    include_directories(SYSTEM ../../thirdparty/pybind11/include)
    include_directories(python)
    add_definitions(-DPYTHON_SUPPORT)

    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script/python")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/pybind11/include")

    target_link_libraries(${testfilename} minifi-script-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})

    createTests("${testfilename}")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}"  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")

    # Copy test resources only once after the first build to be available for all test cases
    if(EXTENSIONS_TEST_COUNT EQUAL 1)
        add_custom_command(
            TARGET "${testfilename}"
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/extensions/script/tests/test_python_scripts"
                    "$<TARGET_FILE_DIR:${testfilename}>/test_python_scripts"
            )
    endif()
ENDFOREACH()

FOREACH(testfile ${EXECUTESCRIPT_LUA_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script/lua")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${SOL2_INCLUDE_DIR}")
    add_definitions(-DLUA_SUPPORT)
    target_link_libraries(${testfilename} minifi-script-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

message("-- Finished building ${EXTENSIONS_TEST_COUNT} ExecuteScript related test file(s)...")
