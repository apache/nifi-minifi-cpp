#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

file(GLOB SQL_MOCK_SOURCES  "mocks/*.cpp")
add_minifi_library(minifi-sql-mocks STATIC ${SQL_MOCK_SOURCES})
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/sql")
target_include_directories(minifi-sql-mocks BEFORE PRIVATE "mocks")
target_wholearchive_library(minifi-sql-mocks core-minifi)

file(GLOB SQL_TESTS  "*.cpp")
set(SQL_TEST_COUNT 0)
foreach(testfile ${SQL_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable(${testfilename} "${testfile}")

    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/sql")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-sql-mocks)
    target_link_libraries(${testfilename} minifi-sql)
    target_link_libraries(${testfilename} minifi-standard-processors)
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})

    math(EXPR SQL_TEST_COUNT "${SQL_TEST_COUNT}+1")
endforeach()

message("-- Finished building ${SQL_TEST_COUNT} sql test file(s)...")
