# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB CIVETWEB_INTEGRATION_TESTS "*.cpp")
SET(CIVETWEB-EXTENSIONS_TEST_COUNT 0)
FOREACH(testfile ${CIVETWEB_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/civetweb")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")

    target_link_libraries(${testfilename} minifi-civet-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_compile_definitions(${testfilename} PRIVATE TEST_RESOURCES="${TEST_RESOURCES}")
    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    createIntegrationTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR CIVETWEB-EXTENSIONS_TEST_COUNT "${CIVETWEB-EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
    set_tests_properties("${testfilename}" PROPERTIES LABELS "civetweb")
ENDFOREACH()

list(GET CIVETWEB_INTEGRATION_TESTS 0 firsttestfile)
get_filename_component(firsttestfilename "${firsttestfile}" NAME_WE)
# Copy test resources
add_custom_command(
    TARGET "${firsttestfilename}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/extensions/civetweb/tests/resources"
            "$<TARGET_FILE_DIR:${firsttestfilename}>/resources"
    )

message("-- Finished building ${CIVETWEB-EXTENSIONS_TEST_COUNT} civetweb related test file(s)...")
