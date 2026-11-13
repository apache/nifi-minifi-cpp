# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (NOT MINIFI_PERFORMANCE_TESTS)
    return()
endif()

include(GetBenchmark)

GETSOURCEFILES(PERF_TESTS "${TEST_DIR}/unit/performance")

SET(PERF_TEST_COUNT 0)
FOREACH(testfile ${PERF_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/performance/${testfile}")
    target_link_libraries(${testfilename} benchmark::benchmark core-minifi)
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    MATH(EXPR PERF_TEST_COUNT "${PERF_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    set_tests_properties(${testfilename} PROPERTIES LABELS "performance")
ENDFOREACH()
message("-- Finished building ${PERF_TEST_COUNT} performance test file(s)...")
