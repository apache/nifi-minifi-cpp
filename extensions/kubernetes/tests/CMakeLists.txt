# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB KUBERNETES_UNIT_TESTS  "*.cpp")
SET(KUBERNETES_UNIT_TEST_COUNT 0)
FOREACH(TEST_FILE ${KUBERNETES_UNIT_TESTS})
    get_filename_component(TEST_TARGET "${TEST_FILE}" NAME_WE)
    add_minifi_executable("${TEST_TARGET}" "${TEST_FILE}")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi")
    target_include_directories(${TEST_TARGET} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/kubernetes")
    target_link_libraries(${TEST_TARGET} minifi-kubernetes-extensions)
    target_link_libraries(${TEST_TARGET} Catch2WithMain)
    target_compile_definitions(${TEST_TARGET} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${TEST_TARGET}")
    add_test(NAME ${TEST_TARGET} COMMAND "${TEST_TARGET}" WORKING_DIRECTORY "${TEST_DIR}")
    MATH(EXPR KUBERNETES_UNIT_TEST_COUNT "${KUBERNETES_UNIT_TEST_COUNT}+1")
    set_tests_properties("${TEST_TARGET}" PROPERTIES LABELS "kubernetes")
ENDFOREACH()
message("-- Finished building ${KUBERNETES_UNIT_TEST_COUNT} Kubernetes related test file(s)...")
