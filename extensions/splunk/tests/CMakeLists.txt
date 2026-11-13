# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB SPLUNK_TESTS  "*.cpp")

SET(SPLUNK_TEST_COUNT 0)
FOREACH(testfile ${SPLUNK_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/splunk")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain civetweb::civetweb-cpp minifi-splunk minifi-standard-processors)
    MATH(EXPR SPLUNK_TEST_COUNT "${SPLUNK_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${SPLUNK_TEST_COUNT} Splunk related test file(s)...")
