# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)
set(CRC32C_USE_GLOG OFF CACHE INTERNAL crc32c-glog-off)
set(CRC32C_BUILD_TESTS OFF CACHE INTERNAL crc32c-gtest-off)
set(CRC32C_BUILD_BENCHMARKS OFF CACHE INTERNAL crc32-benchmarks-off)
set(CRC32C_INSTALL ON CACHE INTERNAL crc32-install-on)
FetchContent_Declare(
        crc32c
        URL     https://github.com/google/crc32c/archive/refs/tags/1.1.2.tar.gz
        URL_HASH SHA256=ac07840513072b7fcebda6e821068aa04889018f24e10e46181068fb214d7e56
        SYSTEM
)
FetchContent_MakeAvailable(crc32c)

if (NOT TARGET Crc32c::crc32c)
    add_library(Crc32c::crc32c ALIAS crc32c)
endif()
