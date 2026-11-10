# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_COUCHBASE_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Couchbase")
    find_package(couchbase_cxx_client REQUIRED)
elseif(MINIFI_COUCHBASE_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Couchbase from source")
    include(Couchbase)
endif()
