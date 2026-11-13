# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_ROCKSDB_SOURCE STREQUAL "CONAN")
    message("Using Conan to install RocksDB")
    find_package(RocksDB REQUIRED)
    if (NOT TARGET RocksDB::RocksDB)
        add_library(RocksDB::RocksDB ALIAS RocksDB::rocksdb)
    endif()
elseif(MINIFI_ROCKSDB_SOURCE STREQUAL "BUILD")
    message("Using CMake to build RocksDB from source")

    if (BUILD_ROCKSDB)
        include(BundledRocksDB)
        use_bundled_rocksdb(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    else()
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/rocksdb/sys")
        find_package(RocksDB REQUIRED)
    endif()
endif()
