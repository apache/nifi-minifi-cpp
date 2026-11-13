# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (ENABLE_GRPC_FOR_LOKI)
    file(GLOB GRAFANA_LOKI_TESTS  "*.cpp")
else()
    set(GRAFANA_LOKI_TESTS  "PushGrafanaLokiRESTTest.cpp")
endif()

SET(GRAFANA_LOKI_TEST_COUNT 0)
FOREACH(testfile ${GRAFANA_LOKI_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    if (MSVC)
        target_compile_options("${testfilename}" PRIVATE /wd4996)
    endif()
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/grafana-loki")
    if (ENABLE_GRPC_FOR_LOKI)
        target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${CMAKE_BINARY_DIR}/grafana-loki-protobuf-generated")
    endif()
    target_include_directories(${testfilename} BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain civetweb::civetweb-cpp minifi-grafana-loki minifi-standard-processors)
    MATH(EXPR GRAFANA_LOKI_TEST_COUNT "${GRAFANA_LOKI_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "grafana-loki")
ENDFOREACH()
message("-- Finished building ${GRAFANA_LOKI_TEST_COUNT} Grafana Loki related test file(s)...")
