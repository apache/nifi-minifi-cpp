# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB EXECUTESCRIPT_PYTHON_TESTS "TestExecuteScriptProcessorWithPythonScript.cpp" "PythonScriptEngineTests.cpp")
file(GLOB EXECUTEPYTHONPROCESSOR_UNIT_TESTS "ExecutePythonProcessorTests.cpp")
file(GLOB PY_SOURCES  "python/*.cpp")
include(GenericPython)

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_PYTHON_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
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
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    target_link_libraries(${testfilename} minifi-python-script-extension minifi-script-extension minifi-standard-processors Catch2WithMain Python::Python)

    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}"  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

add_minifi_executable(PythonManifestTests "PythonManifestTests.cpp")
add_dependencies(PythonManifestTests minifi-python-script-extension minifi-script-extension minifi-standard-processors)
if (NOT WIN32)
    add_dependencies(PythonManifestTests minifi-python-lib-loader-extension)
endif()
target_link_libraries(PythonManifestTests Catch2WithMain)
createTests(PythonManifestTests)
MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
add_test(NAME "PythonManifestTests" COMMAND "PythonManifestTests"  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

copyTestResources(${CMAKE_SOURCE_DIR}/extensions/python/pythonprocessors/nifiapi ${CMAKE_BINARY_DIR}/bin/resources/minifi-python/nifiapi)
copyTestResources(${CMAKE_CURRENT_SOURCE_DIR}/test_python_scripts/ ${CMAKE_BINARY_DIR}/bin/resources/test_python_scripts/)
message("-- Finished building ${EXTENSIONS_TEST_COUNT} tests for minifi-python-script-extension...")
