# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_JSONSCHEMA_VALIDATOR_SOURCE STREQUAL "CONAN")
    message("Using Conan to install json-schema-validator")
    find_package(nlohmann_json_schema_validator REQUIRED)
elseif(MINIFI_JSONSCHEMA_VALIDATOR_SOURCE STREQUAL "BUILD")
    message("Using CMake to build json-schema-validator from source")
    include(JsonSchemaValidator)
endif()
