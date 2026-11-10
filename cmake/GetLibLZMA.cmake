# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LIBLZMA_SOURCE STREQUAL "CONAN")
    message("Using Conan to install LibLZMA")
    find_package(LibLZMA REQUIRED)
elseif(MINIFI_LIBLZMA_SOURCE STREQUAL "BUILD")
    message("Using CMake to build LibLZMA from source")
    include(LibLZMA)
endif()
