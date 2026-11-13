# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB AWS_INTEGRATION_TESTS  "*.cpp")

SET(AWS_TEST_COUNT 0)
FOREACH(testfile ${AWS_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/aws")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/aws/s3")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/aws/processors")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2)
    target_link_libraries(${testfilename} minifi-aws)
    target_link_libraries(${testfilename} minifi-standard-processors)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm64)|(ARM64)|(aarch64)|(armv8)")
        target_link_libraries(${testfilename} AWS::aws-checksums)
    endif()
    target_link_libraries(${testfilename} AWS::aws-cpp-sdk-s3)
    target_link_libraries(${testfilename} AWS::aws-cpp-sdk-s3-crt)
    target_link_libraries(${testfilename} AWS::aws-cpp-sdk-kinesis)
    MATH(EXPR AWS_TEST_COUNT "${AWS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "aws")
ENDFOREACH()
message("-- Finished building ${AWS_TEST_COUNT} AWS related test file(s)...")
