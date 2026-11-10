# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(jolt_tests
        URL      https://github.com/bazaarvoice/jolt/archive/refs/tags/jolt-0.1.8.tar.gz
        URL_HASH SHA256=7423c5b98244260f89a975f5e21150c02a6a1fa88e3af07c90d43fef0eebdcbb
        SYSTEM
        )

FetchContent_MakeAvailable(jolt_tests)
