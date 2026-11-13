# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)
FetchContent_Declare(
        mimalloc
        URL      https://github.com/microsoft/mimalloc/archive/refs/tags/v3.3.2.tar.gz
        URL_HASH SHA256=ca02384e007f46950598500dfaebde5ff9948c1d231f5a81b058799afa64bbbb
        SYSTEM
)
FetchContent_MakeAvailable(mimalloc)
