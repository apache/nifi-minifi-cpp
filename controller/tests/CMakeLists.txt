#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Controller Tests are not supported on windows
if (WIN32)
    return()
endif()

file(GLOB CONTROLLER_TESTS  "*.cpp")

file(GLOB CONTROLLER_SOURCES  "${CMAKE_SOURCE_DIR}/controller/*.cpp")
list(REMOVE_ITEM CONTROLLER_SOURCES "${CMAKE_SOURCE_DIR}/controller/MiNiFiController.cpp")

set(CONTROLLER_TEST_COUNT 0)
foreach(testfile ${CONTROLLER_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable(${testfilename} "${testfile}" ${CONTROLLER_SOURCES})

    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/controller")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")

    createTests(${testfilename})

    target_link_libraries(${testfilename} Catch2WithMain ${LIBMINIFI} libminifi-unittest)
    add_test(NAME ${testfilename} COMMAND ${testfilename} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    math(EXPR CONTROLLER_TEST_COUNT "${CONTROLLER_TEST_COUNT}+1")
endforeach()

list(GET CONTROLLER_TESTS 0 firsttestfile)
get_filename_component(firsttestfilename "${firsttestfile}" NAME_WE)
# Copy test resources
add_custom_command(
    TARGET "${firsttestfilename}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/controller/tests/resources"
            "$<TARGET_FILE_DIR:${firsttestfilename}>/resources"
    )

message("-- Finished building ${CONTROLLER_TEST_COUNT} controller test file(s)...")
