# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
include(FetchContent)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    protobuf
    URL      https://github.com/protocolbuffers/protobuf/archive/refs/tags/v35.0.tar.gz
    URL_HASH SHA256=e127ea69dd7be4e88abdd95845fb6c30d25d96971d95827e92b70e2e910d46a1
)
FetchContent_MakeAvailable(protobuf)

set(PROTOBUF_INCLUDE_DIR "${protobuf_SOURCE_DIR}/src" CACHE STRING "" FORCE)
set(PROTOBUF_COMPILER "$<TARGET_FILE:protoc>" CACHE STRING "" FORCE)

if (WIN32)
    set(PROTOBUF_LIBRARIES "${protobuf_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/protobuf_static.lib" CACHE STRING "" FORCE)
    set(PROTOBUF_LIBRARY "${protobuf_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/protobuf_static.lib" CACHE STRING "" FORCE)
else()
    set(PROTOBUF_LIBRARIES "${protobuf_BINARY_DIR}/lib/libprotobuf.a" CACHE STRING "" FORCE)
    set(PROTOBUF_LIBRARY "${protobuf_BINARY_DIR}/lib/libprotobuf.a" CACHE STRING "" FORCE)
endif()
