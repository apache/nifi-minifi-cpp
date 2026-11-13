# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_MIMALLOC_SOURCE STREQUAL "CONAN")
    message("Using Conan to install mimalloc")
    find_package(mimalloc REQUIRED)
elseif(MINIFI_MIMALLOC_SOURCE STREQUAL "BUILD")
    message("Using CMake to build mimalloc from source")
    include(MiMalloc)
endif()
