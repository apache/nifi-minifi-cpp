# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(GetFmt)

if(MINIFI_SPDLOG_SOURCE STREQUAL "CONAN")
    message("Using Conan to install spdlog")
    find_package(spdlog REQUIRED)

    if (NOT TARGET spdlog)
        add_library(spdlog ALIAS spdlog::spdlog)
    endif()
elseif(MINIFI_SPDLOG_SOURCE STREQUAL "BUILD")
    message("Using CMake to build spdlog from source")
    include(Spdlog)
endif()
