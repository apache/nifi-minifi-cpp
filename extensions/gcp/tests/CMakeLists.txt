# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB GCS_TESTS  "*.cpp")
FOREACH(testfile ${GCS_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")

    if (WIN32)
        target_compile_definitions(${testfilename} PRIVATE _SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING)
    else()
        target_compile_options(${testfilename} PRIVATE -Wno-error=deprecated-declarations)  # Suppress deprecation warnings for std::rel_ops usage
    endif()

    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/gcp")

    if(MINIFI_GCP_SOURCE STREQUAL "BUILD")
        target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_INCLUDE_DIRS})
        target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_SOURCE_DIR}/googletest/include)
        target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_SOURCE_DIR}/googlemock/include)
    endif()

    target_link_libraries(${testfilename} ${MINIFI_GCP_TEST_GTEST_LIBRARIES})

    createTests("${testfilename}")

    target_link_libraries(${testfilename} minifi-gcp)
    target_link_libraries(${testfilename} libminifi-c-unittest)

    target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")

    gtest_add_tests(TARGET "${testfilename}")
ENDFOREACH()
