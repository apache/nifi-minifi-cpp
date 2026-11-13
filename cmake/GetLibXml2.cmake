# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LIBXML2_SOURCE STREQUAL "CONAN")
    message("Using Conan to install libxml2")
    find_package(libxml2 REQUIRED)
elseif(MINIFI_LIBXML2_SOURCE STREQUAL "BUILD")
    message("Using CMake to build libxml2 from source")
    include(LibXml2)
endif()
