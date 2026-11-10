# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_ARGPARSE_SOURCE STREQUAL "CONAN")
    message("Using Conan to install argparse")
    find_package(argparse REQUIRED)
elseif(MINIFI_ARGPARSE_SOURCE STREQUAL "BUILD")
    message("Using CMake to build argparse from source")
    include(ArgParse)
endif()
