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

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/lmdb/add-cmake-file.patch")
if (WIN32)
    set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/lmdb/fix-windows-symbols.patch")
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")
else()
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\")")
endif()

FetchContent_Declare(
        lmdb
        URL      https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_1.0.0-branch.tar.gz
        URL_HASH SHA256=8d3e790194e43a72f172f34c442ea4737b2d1433fc0983f2ef70bae999bc2d28
        PATCH_COMMAND "${PC}"
        SOURCE_SUBDIR "libraries/liblmdb"
        SYSTEM
)

FetchContent_MakeAvailable(lmdb)

set(LMDB_INCLUDE_DIR "${lmdb_SOURCE_DIR}/libraries/liblmdb")
