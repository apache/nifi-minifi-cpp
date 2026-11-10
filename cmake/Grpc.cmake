# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
include(FetchContent)
include(Abseil)
include(Protobuf)

set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(gRPC_ABSL_PROVIDER "package" CACHE STRING "" FORCE)
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)
set(gRPC_SSL_PROVIDER "package" CACHE STRING "" FORCE)
set(gRPC_PROTOBUF_PROVIDER "package" CACHE STRING "" FORCE)

set(PATCH_FILE1 "${CMAKE_SOURCE_DIR}/thirdparty/grpc/all/patches/fix-protobuf-find-package.patch")
set(PATCH_FILE2 "${CMAKE_SOURCE_DIR}/thirdparty/grpc/all/patches/fix-msvc-auto-return-type-template-arg.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE2}\\\")")

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.83.1
  GIT_SUBMODULES "third_party/cares/cares third_party/re2 third_party/upb"
  PATCH_COMMAND "${PC}"
  SYSTEM
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(grpc)

add_dependencies(grpc++ OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

set(GRPC_INCLUDE_DIR "${grpc_SOURCE_DIR}/include" CACHE STRING "" FORCE)
set(GRPC_CPP_PLUGIN "$<TARGET_FILE:grpc_cpp_plugin>" CACHE STRING "" FORCE)

if (NOT TARGET gRPC::grpc++)
    add_library(gRPC::grpc++ ALIAS grpc++)
endif()
