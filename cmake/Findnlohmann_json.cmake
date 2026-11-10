# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT nlohmann_json_FOUND)
    set(nlohmann_json_FOUND "YES" CACHE STRING "" FORCE)
    set(nlohmann_json_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/nlohmann/" CACHE STRING "" FORCE)
    if(NOT EXISTS "${nlohmann_json_INCLUDE_DIR}/nlohmann/json.hpp")
        file(DOWNLOAD "https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp" "${nlohmann_json_INCLUDE_DIR}/nlohmann/json.hpp"
                EXPECTED_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63)
    endif()
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${nlohmann_json_INCLUDE_DIR}")
endif()
