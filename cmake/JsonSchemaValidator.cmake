# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(json-schema-validator
    URL https://github.com/pboettch/json-schema-validator/archive/refs/tags/2.4.0.tar.gz
    URL_HASH SHA256=24cbb114609cc9b43d4018b8d03e082ff5d2f26f5dce8bd36538097267b63af9
    SYSTEM)

FetchContent_MakeAvailable(json-schema-validator)

target_compile_options(nlohmann_json_schema_validator PRIVATE -fsigned-char)
