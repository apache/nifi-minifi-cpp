# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


set(WEL_TESTS  "BookmarkTests.cpp" "ConsumeWindowsEventLogTests.cpp" "LookupCacherTests.cpp" "StringSplitterTests.cpp" "MetadataWalkerTests.cpp" "WindowsErrorTests.cpp")
if (TEST_CUSTOM_WEL_PROVIDER)
    execute_process(COMMAND
        "${CMAKE_CURRENT_LIST_DIR}/custom-provider/generate-and-register.bat"
        "${CMAKE_CURRENT_LIST_DIR}/custom-provider"
    )
    list(APPEND WEL_TESTS "CWELCustomProviderTests.cpp")
endif()

SET(WEL_TEST_COUNT 0)
FOREACH(testfile ${WEL_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/windows-event-log/")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} minifi-wel)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries (${testfilename} ${LIBMINIFI} Catch2WithMain)

    MATH(EXPR WEL_TEST_COUNT "${WEL_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
ENDFOREACH()
message("-- Finished building ${WEL_TEST_COUNT} WEL related test file(s)...")
