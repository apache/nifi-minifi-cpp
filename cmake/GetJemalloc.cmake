# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_JEMALLOC_SOURCE STREQUAL "CONAN")
    message("Using Conan to install jemalloc")
    find_package(jemalloc REQUIRED)
elseif(MINIFI_JEMALLOC_SOURCE STREQUAL "BUILD")
    message("Using CMake to build jemalloc from source")
    include(BundledJemalloc)
    use_bundled_jemalloc(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
