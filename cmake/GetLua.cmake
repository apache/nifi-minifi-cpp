# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_LUA_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Lua")
    find_package(lua REQUIRED)
elseif(MINIFI_LUA_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Lua from source")
    include(Lua)
endif()
