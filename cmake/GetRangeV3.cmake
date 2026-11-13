# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_RANGEV3_SOURCE STREQUAL "CONAN")
    message("Using Conan to install range-v3")
    find_package(range-v3 REQUIRED)
elseif(MINIFI_RANGEV3_SOURCE STREQUAL "BUILD")
    message("Using CMake to build range-v3 from source")
    include(RangeV3)
endif()
