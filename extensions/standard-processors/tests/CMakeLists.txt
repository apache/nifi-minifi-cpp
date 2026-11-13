# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


include(GetJoltTests)

file(GLOB PROCESSOR_UNIT_TESTS  "unit/*.cpp" "unit/modbus/*.cpp")
file(GLOB PROCESSOR_INTEGRATION_TESTS "integration/*.cpp")

SET(PROCESSOR_INT_TEST_COUNT 0)

FOREACH(testfile ${PROCESSOR_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE ${PROCESSOR_INCLUDE_DIRS})
    target_include_directories(${testfilename} BEFORE PRIVATE "../")
    target_include_directories(${testfilename} BEFORE PRIVATE "../processors")
    target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    target_include_directories(${testfilename} BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_compile_definitions("${testfilename}" PRIVATE JOLT_TESTS_DIR="${jolt_tests_SOURCE_DIR}/jolt-core/src/test/resources/json/shiftr")
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    target_compile_definitions("${testfilename}" PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    if (ENABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    MATH(EXPR PROCESSOR_INT_TEST_COUNT "${PROCESSOR_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${PROCESSOR_UNIT_TESTS} processor unit test file(s)...")


SET(INT_TEST_COUNT 0)
FOREACH(testfile ${PROCESSOR_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE ${PROCESSOR_INCLUDE_DIRS})
    target_include_directories(${testfilename} BEFORE PRIVATE "../")
    target_include_directories(${testfilename} BEFORE PRIVATE "../processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "../http/")
    target_include_directories(${testfilename} BEFORE PRIVATE "../protocols/")
    target_include_directories(${testfilename} BEFORE PRIVATE "../sitetosite/")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
    target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    createIntegrationTests("${testfilename}")
    target_link_libraries(${testfilename})
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    target_compile_definitions("${testfilename}" PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    if (ENABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")

    MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

copyTestResources(${CMAKE_SOURCE_DIR}/libminifi/test/resources/certs ${CMAKE_BINARY_DIR}/bin/resources)
