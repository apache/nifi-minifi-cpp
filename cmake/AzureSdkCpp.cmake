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

if (WIN32)
    set(WIL_BUILD_TESTS OFF CACHE INTERNAL "")
    set(WIL_BUILD_PACKAGING OFF CACHE INTERNAL "")

    FetchContent_Declare(
            wil
            URL      https://github.com/microsoft/wil/archive/refs/tags/v1.0.250325.1.tar.gz
            URL_HASH SHA256=c9e667d5f86ded43d17b5669d243e95ca7b437e3a167c170805ffd4aa8a9a786
            SYSTEM
    )
    FetchContent_MakeAvailable(wil)
endif()

set(WARNINGS_AS_ERRORS OFF CACHE INTERNAL "")
set(DISABLE_AZURE_CORE_OPENTELEMETRY ON CACHE INTERNAL "")
set(BUILD_TRANSPORT_CURL ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SAMPLES OFF CACHE INTERNAL "")
set(DISABLE_AMQP ON CACHE INTERNAL "")

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/azure-sdk-cpp/wil.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/azure-sdk-cpp/fix-openssl-helper.patch")
set(PATCH_FILE_3 "${CMAKE_SOURCE_DIR}/thirdparty/azure-sdk-cpp/fix-managed-identity.patch")
set(PATCH_FILE_4 "${CMAKE_SOURCE_DIR}/thirdparty/azure-sdk-cpp/include-cinttypes-for-uint8_t-gcc15-fix.patch")

set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_3}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_4}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_4}\\\")")

FetchContent_Declare(asdkext
    URL https://github.com/Azure/azure-sdk-for-cpp/archive/refs/tags/azure-identity_1.13.0.tar.gz
    URL_HASH "SHA256=a18ce70f8da11266b6a1aec3c02233ca889ea6137471a9f633131f00dfd386c0"
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(asdkext)
