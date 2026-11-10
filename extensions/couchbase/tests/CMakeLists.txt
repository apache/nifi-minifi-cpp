# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB COUCHBASE_TESTS  "*.cpp")

SET(COUCHBASE_TEST_COUNT 0)
FOREACH(testfile ${COUCHBASE_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/couchbase/controllerservices")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/couchbase/processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/couchbase/")
    target_include_directories(${testfilename} PRIVATE BEFORE "${COUCHBASE_INCLUDE_DIR}")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    # Demote preprocessor warnings so <ciso646> doesn't break the build
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${testfilename} PRIVATE -Wno-error=cpp)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${testfilename} PRIVATE -Wno-error=\#warnings)
    endif()
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-couchbase)
    target_link_libraries(${testfilename} minifi-standard-processors)
    MATH(EXPR COUCHBASE_TEST_COUNT "${COUCHBASE_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${COUCHBASE_TEST_COUNT} Couchbase related test file(s)...")
