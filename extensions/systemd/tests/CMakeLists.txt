# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


function(add_systemd_test testfile)
    get_filename_component(TEST_TARGET "${testfile}" NAME_WE)
    add_minifi_executable(${TEST_TARGET} "${testfile}")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/systemd/")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_link_libraries(${TEST_TARGET} minifi-systemd)
    target_link_libraries(${TEST_TARGET} minifi-standard-processors)
    createTests("${TEST_TARGET}")
    add_test(NAME ${TEST_TARGET} COMMAND "${TEST_TARGET}" WORKING_DIRECTORY "${TEST_DIR}")
    target_link_libraries(${TEST_TARGET} Catch2WithMain)
endfunction()

add_systemd_test("ConsumeJournaldTest.cpp")
