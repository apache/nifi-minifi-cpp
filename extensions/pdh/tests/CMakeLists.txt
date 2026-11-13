# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB PDH_TESTS  "*.cpp")

SET(PDH_TEST_COUNT 0)
FOREACH(testfile ${PDH_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/pdh")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-pdh)
    target_link_libraries(${testfilename} minifi-standard-processors)
    MATH(EXPR PDH_TEST_COUNT "${PDH_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "pdh")
ENDFOREACH()
message("-- Finished building ${PDH_TEST_COUNT} PDH related test file(s)...")
