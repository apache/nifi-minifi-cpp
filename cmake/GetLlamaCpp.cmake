# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LLAMACPP_SOURCE STREQUAL "CONAN")
    message("Using Conan to install LlamaCpp")
    find_package(llamacpp REQUIRED)
elseif(MINIFI_LLAMACPP_SOURCE STREQUAL "BUILD")
    message("Using CMake to build LlamaCpp from source")
    include(LlamaCpp)
endif()
