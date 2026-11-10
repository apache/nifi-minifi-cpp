# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_ZSTD_SOURCE STREQUAL "CONAN")
    message("Using Conan to install zstd")
    find_package(zstd REQUIRED)
    if(NOT TARGET zstd::zstd)
        add_library(zstd::zstd ALIAS zstd::libzstd_static)
    endif()
    set(ZSTD_LIBRARIES "${zstd_LIBRARIES}" CACHE STRING "" FORCE)
    set(ZSTD_INCLUDE_DIRS "${zstd_INCLUDE_DIRS}" CACHE STRING "" FORCE)
elseif(MINIFI_ZSTD_SOURCE STREQUAL "BUILD")
    message("Using CMake to build zstd from source")
    include(Zstd)
endif()
