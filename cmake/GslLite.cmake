# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(gsl-lite
    URL      https://github.com/gsl-lite/gsl-lite/archive/refs/tags/v1.1.0.tar.gz
    URL_HASH SHA256=bc786258eda511980942b973f5f394c854705bb0462ee46f82a8c39b5e19ea03
    SYSTEM
)
FetchContent_MakeAvailable(gsl-lite)
