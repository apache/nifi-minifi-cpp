# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT ENABLE_ROCKSDB)
    return()
endif()

include(GetRocksDB)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "controllers/*.cpp" "database/*.cpp" "encryption/*.cpp")

add_minifi_library(minifi-rocksdb-repos SHARED ${SOURCES})

target_link_libraries(minifi-rocksdb-repos PUBLIC RocksDB::RocksDB)
target_link_libraries(minifi-rocksdb-repos PUBLIC minifi-api minifi-extension-framework Threads::Threads)
target_link_libraries(minifi-rocksdb-repos PRIVATE $<LINK_ONLY:core-minifi>)

register_extension(minifi-rocksdb-repos "ROCKSDB REPOS" ROCKSDB-REPOS "This Enables persistent provenance, flowfile, and content repositories using RocksDB" "extensions/rocksdb-repos/tests")
