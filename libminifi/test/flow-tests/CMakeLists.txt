# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB FLOW_TESTS  "*.cpp")

SET(FLOW_TEST_COUNT 0)

FOREACH(testfile ${FLOW_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
    target_compile_definitions("${testfilename}" PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} minifi-standard-processors)
    if (ENABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()

    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR FLOW_TEST_COUNT "${FLOW_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
ENDFOREACH()
message("-- Finished building ${FLOW_TEST_COUNT} flow test file(s)...")
