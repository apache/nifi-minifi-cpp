# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB PERSISTENCE_TESTS  "*.cpp")
SET(PERSISTENCE_TEST_COUNT 0)
FOREACH(testfile ${PERSISTENCE_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/libarchive")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/rocksdb-repos/")
    target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE "${ROCKSDB_THIRDPARTY_ROOT}/include")
    target_link_libraries(${testfilename} minifi-archive-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} minifi-rocksdb-repos)
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR PERSISTENCE_TEST_COUNT "${PERSISTENCE_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${ARCHIVE-PERSISTENCE_TEST_COUNT} Persistence related test file(s)...")
