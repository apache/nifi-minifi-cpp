#
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
#
include(FetchContent)
set(CRC32C_USE_GLOG OFF CACHE INTERNAL crc32c-glog-off)
set(CRC32C_BUILD_TESTS OFF CACHE INTERNAL crc32c-gtest-off)
set(CRC32C_BUILD_BENCHMARKS OFF CACHE INTERNAL crc32-benchmarks-off)
set(CRC32C_INSTALL ON CACHE INTERNAL crc32-install-on)
FetchContent_Declare(
        crc32c
        URL     https://github.com/google/crc32c/archive/refs/tags/1.1.2.tar.gz
        URL_HASH SHA256=ac07840513072b7fcebda6e821068aa04889018f24e10e46181068fb214d7e56
)
FetchContent_MakeAvailable(crc32c)
add_library(Crc32c::crc32c ALIAS crc32c)
