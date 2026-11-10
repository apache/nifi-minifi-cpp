# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_JOLT_TESTS_SOURCE STREQUAL "CONAN")
    message("Using Conan to install jolt test resources")
    find_package(jolt-tests REQUIRED)
elseif(MINIFI_JOLT_TESTS_SOURCE STREQUAL "BUILD")
    message("Using CMake to fetch jolt test resources from source")
    include(JoltTests)
endif()
