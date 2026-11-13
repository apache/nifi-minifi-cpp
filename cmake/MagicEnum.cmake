# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(magic_enum
        URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.8.tar.gz
        URL_HASH SHA256=1e54959a3f3cb675938d858603ad69d0f3f7c82439fc2bf86d7232daec2bd10e
        SYSTEM)

FetchContent_MakeAvailable(magic_enum)
