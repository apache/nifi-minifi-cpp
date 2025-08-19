# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
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

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/grpc/fix-protobuf-find-package.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.72.2
  GIT_SUBMODULES "third_party/cares/cares third_party/re2 third_party/upb"
  PATCH_COMMAND "${PC}"
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(grpc)

add_dependencies(grpc++ OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

set(GRPC_INCLUDE_DIR "${grpc_SOURCE_DIR}/include" CACHE STRING "" FORCE)
set(GRPC_CPP_PLUGIN "$<TARGET_FILE:grpc_cpp_plugin>" CACHE STRING "" FORCE)
