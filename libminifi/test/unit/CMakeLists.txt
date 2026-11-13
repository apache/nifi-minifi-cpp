# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")

if (NOT WIN32)
    list(REMOVE_ITEM UNIT_TESTS WindowsCertStoreLocationTests.cpp)
endif()

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}")
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    target_compile_definitions("${testfilename}" PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    target_compile_definitions("${testfilename}" PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")

add_subdirectory(performance)
