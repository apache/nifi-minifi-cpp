# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LIBSODIUM_SOURCE STREQUAL "CONAN")
    message("Using Conan to install libsodium")
    find_package(LibSodium REQUIRED)
elseif(MINIFI_LIBSODIUM_SOURCE STREQUAL "BUILD")
    message("Using CMake to build libsodium from source")
    include(BundledLibSodium)
    use_bundled_libsodium(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
