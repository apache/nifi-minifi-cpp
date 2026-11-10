# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LIBSSH2_SOURCE STREQUAL "CONAN")
    message("Using Conan to install LibSSH2")
    find_package(Libssh2 REQUIRED)
elseif(MINIFI_LIBSSH2_SOURCE STREQUAL "BUILD")
    message("Using CMake to build LibSSH2 from source")
    include(FetchLibSSH2)
endif()
