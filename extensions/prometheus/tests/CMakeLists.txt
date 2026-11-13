# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB PROMETHEUS_TESTS  "*.cpp")

SET(PROMETHEUS_TEST_COUNT 0)
FOREACH(testfile ${PROMETHEUS_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/prometheus")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-prometheus)
    MATH(EXPR PROMETHEUS_TEST_COUNT "${PROMETHEUS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    set_tests_properties("${testfilename}" PROPERTIES LABELS "prometheus")
ENDFOREACH()
message("-- Finished building ${PROMETHEUS_TEST_COUNT} Prometheus related test file(s)...")
