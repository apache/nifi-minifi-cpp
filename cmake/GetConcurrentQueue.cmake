# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_CONCURRENTQUEUE_SOURCE STREQUAL "CONAN")
    message("Using Conan to install concurrentqueue")
    find_package(concurrentqueue REQUIRED)
elseif(MINIFI_CONCURRENTQUEUE_SOURCE STREQUAL "BUILD")
    message("Using CMake to build concurrentqueue from source")
    include(ConcurrentQueue)
endif()
