# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_OSSP_UUID_SOURCE STREQUAL "CONAN")
    message("Using Conan to install ossp-uuid")
    find_package(ossp-uuid REQUIRED)
elseif(MINIFI_OSSP_UUID_SOURCE STREQUAL "BUILD")
    message("Using CMake to build ossp-uuid from source")
    include(BundledOSSPUUID)
    use_bundled_osspuuid(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
