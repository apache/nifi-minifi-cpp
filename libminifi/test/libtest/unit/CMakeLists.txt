# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
file(GLOB TEST_BASE_SOURCES "*.cpp")
add_minifi_library(libminifi-unittest STATIC ${TEST_BASE_SOURCES})
target_link_libraries(libminifi-unittest core-minifi minifi-extension-framework Catch2WithMain)
target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
target_compile_definitions(libminifi-unittest PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
if(WIN32)
    target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/win")
else()
    target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/posix")
endif()
