# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_RAPIDJSON_SOURCE STREQUAL "CONAN")
    message("Using Conan to install RapidJSON")
    find_package(RapidJSON REQUIRED)
    target_compile_definitions(rapidjson INTERFACE RAPIDJSON_HAS_STDSTRING=1)
elseif(MINIFI_RAPIDJSON_SOURCE STREQUAL "BUILD")
    message("Using CMake to build RapidJSON from source")
    include(RapidJSON)
endif()
