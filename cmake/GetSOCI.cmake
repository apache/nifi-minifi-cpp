# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_SOCI_SOURCE STREQUAL "CONAN")
    message("Using Conan to install SOCI")
    find_package(SOCI REQUIRED)
    if(NOT TARGET SOCI::SOCI)
        add_library(SOCI::SOCI INTERFACE IMPORTED)
        target_link_libraries(SOCI::SOCI INTERFACE SOCI::soci_core_static SOCI::soci_odbc_static)
    endif()
elseif(MINIFI_SOCI_SOURCE STREQUAL "BUILD")
    message("Using CMake to build SOCI from source")
    include(Soci)
endif()
