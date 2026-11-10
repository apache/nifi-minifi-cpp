# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_SOL2_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Sol2")
    find_package(sol2 REQUIRED)
elseif(MINIFI_SOL2_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Sol2 from source")
    include(Sol2)
endif()
