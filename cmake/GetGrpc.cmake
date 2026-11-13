# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_GRPC_SOURCE STREQUAL "CONAN")
    message("Using Conan to install gRPC")
    find_package(protobuf CONFIG REQUIRED)
    find_package(gRPC CONFIG REQUIRED)
    set(PROTOBUF_INCLUDE_DIR "${protobuf_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(GRPC_INCLUDE_DIR "${gRPC_INCLUDE_DIR}" CACHE STRING "" FORCE)
elseif(MINIFI_GRPC_SOURCE STREQUAL "BUILD")
    message("Using CMake to build gRPC from source")
    include(Grpc)
endif()
