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

set(OPEN62541_VERSION "v1.5.4" CACHE STRING "" FORCE)
set(UA_ENABLE_ENCRYPTION ON CACHE BOOL "" FORCE)
set(UA_FORCE_WERROR OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_DEBUG_SANITIZER OFF CACHE BOOL "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/open62541/open62541.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/open62541/cflag_fix.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

FetchContent_Declare(
    open62541
    URL "https://github.com/open62541/open62541/archive/refs/tags/${OPEN62541_VERSION}.tar.gz"
    URL_HASH "SHA256=fb5aafc19c67a91368d1f71d9ee4acf0f4b47a0d65c66db4ed738691828779c7"
    PATCH_COMMAND "${PC}"
    EXCLUDE_FROM_ALL
    DOWNLOAD_NO_PROGRESS TRUE
    TLS_VERIFY TRUE
    SYSTEM
)

FetchContent_MakeAvailable(open62541)

add_dependencies(open62541 mbedtls)
