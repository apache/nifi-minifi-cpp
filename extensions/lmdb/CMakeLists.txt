# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR MINIFI_LMDB))
    return()
endif()

include(GetLMDB)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-lmdb SHARED ${SOURCES})

target_include_directories(minifi-lmdb PUBLIC ${LMDB_INCLUDE_DIR})
target_link_libraries(minifi-lmdb PUBLIC lmdb::lmdb)
target_link_libraries(minifi-lmdb PUBLIC minifi-api minifi-extension-framework)
target_link_libraries(minifi-lmdb PRIVATE $<LINK_ONLY:core-minifi>)

register_extension(minifi-lmdb "LMDB" LMDB "This enables persistent repositories using LMDB" "extensions/lmdb/tests")
