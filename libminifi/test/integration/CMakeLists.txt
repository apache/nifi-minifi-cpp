# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

SET(TESTS_WITH_LIBARCHIVE_DEPENDENCY C2DebugBundleTest)
SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}")
    createIntegrationTests("${testfilename}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} Catch2WithMain Threads::Threads)
    target_compile_definitions(${testfilename} PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    if(NOT ${testfilename} IN_LIST TESTS_WITH_LIBARCHIVE_DEPENDENCY)
        add_test(NAME "${testfilename}" COMMAND "${testfilename}")
        set_tests_properties("${testfilename}" PROPERTIES LABELS "integration")
    endif()
    MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()

FOREACH(testfilename ${TESTS_WITH_LIBARCHIVE_DEPENDENCY})
    if (ENABLE_LIBARCHIVE)
        add_test(NAME "${testfilename}" COMMAND "${testfilename}")
        set_tests_properties("${testfilename}" PROPERTIES LABELS "integration")
    endif()
ENDFOREACH()

target_link_libraries(OnScheduleErrorHandlingTests minifi-test-processors)

message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

add_subdirectory(extension-verification-test)
