# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
include(FetchContent)

set(RAPIDJSON_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_DOC      OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    rapidjson_src
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG        297637bc0a0f00dc504dbc1a47ebdaa577f8ae19
    SYSTEM
)
FetchContent_MakeAvailable(rapidjson_src)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson SYSTEM INTERFACE ${rapidjson_src_SOURCE_DIR}/include)
target_compile_definitions(rapidjson INTERFACE RAPIDJSON_HAS_STDSTRING=1)
