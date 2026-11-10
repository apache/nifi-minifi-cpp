# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB MINIFI_MAIN_UNIT_TESTS  "*.cpp")
SET(MINIFI_MAIN_UNIT_TEST_COUNT 0)
FOREACH(TEST_FILE ${MINIFI_MAIN_UNIT_TESTS})
    get_filename_component(TEST_TARGET "${TEST_FILE}" NAME_WE)
    add_minifi_executable("${TEST_TARGET}" "${TEST_FILE}" "../TableFormatter.cpp")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/minifi_main")
    target_link_libraries(${TEST_TARGET} core-minifi Catch2WithMain)
    createTests("${TEST_TARGET}")
    add_test(NAME ${TEST_TARGET} COMMAND "${TEST_TARGET}" WORKING_DIRECTORY "${TEST_DIR}")
    MATH(EXPR MINIFI_MAIN_UNIT_TEST_COUNT "${MINIFI_MAIN_UNIT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${MINIFI_MAIN_UNIT_TEST_COUNT} MiNiFi main unit test file(s)...")
copyTestResources(${CMAKE_SOURCE_DIR}/libminifi/test/resources/certs ${CMAKE_BINARY_DIR}/bin/resources)
