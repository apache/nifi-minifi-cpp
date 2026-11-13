# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(Fmt
        URL  https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
        URL_HASH SHA256=ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea
        OVERRIDE_FIND_PACKAGE
        SYSTEM
        )
FetchContent_MakeAvailable(Fmt)
