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

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    protobuf
    URL      https://github.com/protocolbuffers/protobuf/archive/refs/tags/v31.1.tar.gz
    URL_HASH SHA256=c3a0a9ece8932e31c3b736e2db18b1c42e7070cd9b881388b26d01aa71e24ca2
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
