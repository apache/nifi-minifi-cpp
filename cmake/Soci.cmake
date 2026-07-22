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

set(PATCH_FILE1 "${CMAKE_SOURCE_DIR}/thirdparty/soci/all/patches/disable-sqlwchar-support.patch")
set(PATCH_FILE2 "${CMAKE_SOURCE_DIR}/thirdparty/soci/all/patches/odbc-get-parameter-name-bounds-safe.patch")
set(PC ${Bash_EXECUTABLE} -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE2}\\\")")

set(SOCI_TESTS OFF CACHE BOOL "" FORCE)
set(SOCI_SHARED OFF CACHE BOOL "" FORCE)
set(SOCI_ODBC ON CACHE BOOL "" FORCE)
set(SOCI_SQLITE3 OFF CACHE BOOL "" FORCE)
set(SOCI_LTO OFF CACHE BOOL "" FORCE)
set(WITH_BOOST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    soci
    URL "https://github.com/SOCI/soci/archive/refs/tags/v4.1.4.tar.gz"
    URL_HASH "SHA256=144f017cccc2e2d806badb3313d6ab3c67a1925bccaa747a46fb3907108a615d"
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(soci)

if(NOT WIN32)
    add_dependencies(soci_core ODBC::ODBC)
endif()
