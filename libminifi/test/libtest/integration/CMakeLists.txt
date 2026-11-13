# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
file(GLOB SOURCES "*.cpp")
add_minifi_library(libminifi-integrationtest STATIC ${SOURCES})
target_link_libraries(libminifi-integrationtest libminifi-unittest civetweb::civetweb-cpp)
target_include_directories(libminifi-integrationtest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/" "${CMAKE_SOURCE_DIR}/libminifi/test/libtest/")
target_include_directories(libminifi-integrationtest BEFORE PRIVATE "${CIVETWEB_INCLUDE_DIRS}")
