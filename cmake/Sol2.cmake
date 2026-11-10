# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(sol2
    URL      https://github.com/ThePhD/sol2/archive/refs/tags/v3.5.0.tar.gz
    URL_HASH SHA256=86c0f6d2836b184a250fc2907091c076bf53c9603dd291eaebade36cc342e13c
    SYSTEM
)
FetchContent_MakeAvailable(sol2)

set(sol2_INCLUDE_DIR "${sol2_BINARY_DIR}/include" CACHE STRING "" FORCE)
