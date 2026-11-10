# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


add_minifi_executable(EchoParameters resource_apps/EchoParameters.cpp)
set_target_properties(EchoParameters PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_minifi_executable(ExecuteProcessTests ExecuteProcessTests.cpp)

target_link_libraries(ExecuteProcessTests minifi-execute-process minifi-standard-processors Catch2WithMain)
target_include_directories(ExecuteProcessTests PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test")
target_compile_definitions(ExecuteProcessTests PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

add_test(NAME ExecuteProcessTests COMMAND ExecuteProcessTests WORKING_DIRECTORY ${TEST_DIR})
set_tests_properties(ExecuteProcessTests PROPERTIES LABELS "execute-process")

createTests(ExecuteProcessTests)

add_dependencies(ExecuteProcessTests EchoParameters)
