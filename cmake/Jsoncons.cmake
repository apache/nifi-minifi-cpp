# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(jsoncons
    URL      https://github.com/danielaparker/jsoncons/archive/refs/tags/v1.7.0.tar.gz
    URL_HASH SHA256=5a2aad4e791a1c93b0b0b326973459753a96a4c48d06d3035cd0ea0d262198d4
    SYSTEM
)

FetchContent_MakeAvailable(jsoncons)

if (NOT TARGET jsoncons::jsoncons)
    add_library(jsoncons::jsoncons ALIAS jsoncons)
endif()
