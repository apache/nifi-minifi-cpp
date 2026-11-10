# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(
        Catch2
        URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.0.tar.gz
        URL_HASH SHA256=9650c55e497759cc39b977e45524bc8acb15256061c112080916ab6cb0b1ea66
        SYSTEM
)
FetchContent_MakeAvailable(Catch2)
