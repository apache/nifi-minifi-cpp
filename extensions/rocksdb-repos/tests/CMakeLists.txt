# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB ROCKSDB_UNIT_TESTS  "*.cpp")
SET(ROCKSDB_TEST_COUNT 0)
FOREACH(testfile ${ROCKSDB_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/rocksdb-repos/")
    target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE "${ROCKSDB_THIRDPARTY_ROOT}/include")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-rocksdb-repos)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    MATH(EXPR ROCKSDB_TEST_COUNT "${ROCKSDB_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${ROCKSDB_TEST_COUNT} RocksDB related test file(s)...")
