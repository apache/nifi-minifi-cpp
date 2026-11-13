# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
include(FetchContent)

FetchContent_Declare(
    concurrentqueue
    URL           https://github.com/cameron314/concurrentqueue/archive/refs/tags/v1.0.5.tar.gz
    URL_HASH      SHA256=4d6368a27492d86011fde5ca0cf386dce7c49cd425aa3d9b063ca6ec373a6ef3
    SOURCE_DIR    ${CMAKE_BINARY_DIR}/concurrentqueue_include/moodycamel
    SOURCE_SUBDIR does-not-exist   # don't run upstream CMake; we define the target
)
FetchContent_MakeAvailable(concurrentqueue)

if(NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue INTERFACE)
    target_include_directories(concurrentqueue SYSTEM INTERFACE
        ${CMAKE_BINARY_DIR}/concurrentqueue_include)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
endif()
