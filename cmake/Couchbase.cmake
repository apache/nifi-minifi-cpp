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

include(fmt)
include(Spdlog)
include(Asio)

set(COUCHBASE_CXX_CLIENT_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_POST_LINKED_OPENSSL ON CACHE BOOL "" FORCE)
set(COUCHBASE_CXX_CLIENT_INSTALL OFF CACHE BOOL "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/couchbase/remove-thirdparty.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/couchbase/remove-debug-symbols.patch")
set(PATCH_FILE_3 "${CMAKE_SOURCE_DIR}/thirdparty/couchbase/c++23_fixes.patch")

set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_3}\\\")")

FetchContent_Declare(couchbase-cxx-client
    URL      https://github.com/couchbase/couchbase-cxx-client/releases/download/1.1.0/couchbase-cxx-client-1.1.0.tar.gz
    URL_HASH SHA256=bd3a7f1492e242b239acd965eed4472cecb0319d40d05480f97cdec705960ba0
    PATCH_COMMAND "${PC}"
    SYSTEM
)
FetchContent_MakeAvailable(couchbase-cxx-client)

set(COUCHBASE_INCLUDE_DIR "${couchbase-cxx-client_SOURCE_DIR}" CACHE STRING "" FORCE)
