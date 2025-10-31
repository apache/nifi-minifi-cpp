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

file(GLOB EXECUTESCRIPT_PYTHON_TESTS "TestExecuteScriptProcessorWithPythonScript.cpp" "PythonScriptEngineTests.cpp")
file(GLOB EXECUTEPYTHONPROCESSOR_UNIT_TESTS "ExecutePythonProcessorTests.cpp")
file(GLOB PY_SOURCES  "python/*.cpp")
include(GenericPython)

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_PYTHON_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")
    target_link_libraries(${testfilename} minifi-python-script-extension minifi-script-extension Catch2WithMain Python::Python)
    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

FOREACH(testfile ${EXECUTEPYTHONPROCESSOR_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")

    target_include_directories("${testfilename}" PRIVATE "${PYTHON_INCLUDE_DIR}")
    target_include_directories("${testfilename}" PRIVATE python)

    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")

    target_link_libraries(${testfilename} minifi-python-script-extension minifi-script-extension minifi-standard-processors Catch2WithMain Python::Python)

    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}"  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

add_minifi_executable(PythonManifestTests "PythonManifestTests.cpp")
add_dependencies(PythonManifestTests minifi-python-script-extension minifi-script-extension minifi-standard-processors)
target_link_libraries(PythonManifestTests Catch2WithMain)
createTests(PythonManifestTests)
MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
add_test(NAME "PythonManifestTests" COMMAND "PythonManifestTests"  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

copyTestResources(${CMAKE_SOURCE_DIR}/extensions/python/pythonprocessors/nifiapi ${CMAKE_BINARY_DIR}/bin/resources/minifi-python/nifiapi)
copyTestResources(${CMAKE_CURRENT_SOURCE_DIR}/test_python_scripts/ ${CMAKE_BINARY_DIR}/bin/resources/test_python_scripts/)
message("-- Finished building ${EXTENSIONS_TEST_COUNT} tests for minifi-python-script-extension...")
