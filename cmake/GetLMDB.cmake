# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LMDB_SOURCE STREQUAL "CONAN")
    message("Using Conan to install LMDB")
    find_package(lmdb REQUIRED)
elseif(MINIFI_LMDB_SOURCE STREQUAL "BUILD")
    message("Using CMake to build LMDB from source")
    include(LMDB)
endif()
