# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(SPDLOG_FMT_EXTERNAL ON CACHE STRING "" FORCE)
set(SPDLOG_SYSTEM_INCLUDES ON CACHE STRING "" FORCE)

FetchContent_Declare(Spdlog
        URL  https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
        OVERRIDE_FIND_PACKAGE
        SYSTEM
        )
FetchContent_MakeAvailable(Spdlog)
