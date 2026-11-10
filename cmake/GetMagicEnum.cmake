# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_MAGIC_ENUM_SOURCE STREQUAL "CONAN")
    message("Using Conan to install magic_enum")
    find_package(magic_enum REQUIRED)
elseif(MINIFI_MAGIC_ENUM_SOURCE STREQUAL "BUILD")
    message("Using CMake to build magic_enum from source")
    include(MagicEnum)
endif()
