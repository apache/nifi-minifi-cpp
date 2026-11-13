# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_BZIP2_SOURCE STREQUAL "CONAN")
    message("Using Conan to install bzip2")
    find_package(BZip2 REQUIRED)
elseif(MINIFI_BZIP2_SOURCE STREQUAL "BUILD")
    message("Using CMake to build bzip2 from source")
    include(BundledBZip2)
    use_bundled_bzip2(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
