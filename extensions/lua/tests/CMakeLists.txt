# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB EXECUTESCRIPT_LUA_TESTS "*.cpp")

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_LUA_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/lua")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    target_link_libraries(${testfilename} minifi-lua-script-extension minifi-script-extension)
    target_link_libraries(${testfilename} Catch2WithMain)
    createTests("${testfilename}")
    MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "lua")
ENDFOREACH()

copyTestResources(${CMAKE_CURRENT_SOURCE_DIR}/test_lua_scripts/ ${CMAKE_BINARY_DIR}/bin/resources/test_lua_scripts/)
message("-- Finished building ${EXTENSIONS_TEST_COUNT} tests for minifi-lua-script-extension...")
