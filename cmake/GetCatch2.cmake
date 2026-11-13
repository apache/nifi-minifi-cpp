# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_CATCH2_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Catch2")
    find_package(Catch2 REQUIRED)
    if (NOT TARGET Catch2WithMain)
        add_library(Catch2WithMain ALIAS Catch2::Catch2WithMain)
    endif()
    if (NOT TARGET Catch2)
        add_library(Catch2 ALIAS Catch2::Catch2)
    endif()
elseif(MINIFI_CATCH2_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Catch2 from source")
    include(Catch2)
endif()
