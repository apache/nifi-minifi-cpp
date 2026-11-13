# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LIBARCHIVE_SOURCE STREQUAL "CONAN")
    message("Using Conan to install LibArchive")
    find_package(LibArchive REQUIRED)
elseif(MINIFI_LIBARCHIVE_SOURCE STREQUAL "BUILD")
    message("Using CMake to build LibArchive from source")
    include(BundledLibArchive)
    use_bundled_libarchive(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
