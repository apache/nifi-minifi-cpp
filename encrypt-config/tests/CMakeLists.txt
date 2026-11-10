#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
