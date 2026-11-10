# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LZ4_SOURCE STREQUAL "CONAN")
    message("Using Conan to install lz4")
    find_package(lz4 REQUIRED)
    if(NOT TARGET lz4::lz4)
        add_library(lz4::lz4 ALIAS LZ4::liblz4_static)
    endif()
    if(NOT TARGET LZ4::LZ4)
        add_library(LZ4::LZ4 ALIAS lz4::lz4)
    endif()
    set(LZ4_LIBRARIES "${lz4_LIBRARIES}" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIRS "${lz4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
elseif(MINIFI_LZ4_SOURCE STREQUAL "BUILD")
    message("Using CMake to build lz4 from source")
    include(LZ4)
endif()
