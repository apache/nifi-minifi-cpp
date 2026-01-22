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
include(Abseil)
include(Crc32c)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/remove-find_package.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/nlohmann_lib_as_interface.patch")
set(PATCH_FILE_3 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/c++23_fixes.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_3}\\\")")

set(GOOGLE_CLOUD_CPP_WITH_MOCKS OFF CACHE BOOL "" FORCE)
if (NOT SKIP_TESTS)
    include(GoogleTest)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL            https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
            URL_HASH       SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
            SYSTEM
    )
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(GOOGLE_CLOUD_CPP_WITH_MOCKS ON CACHE BOOL "" FORCE)
endif()

set(GOOGLE_CLOUD_CPP_ENABLE storage CACHE INTERNAL storage-api)
set(GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK OFF CACHE INTERNAL macos-openssl-check)
set(BUILD_TESTING OFF CACHE INTERNAL testing-off)
set(GOOGLE_CLOUD_CPP_ENABLE_WERROR OFF CACHE INTERNAL warnings-off)
FetchContent_Declare(google-cloud-cpp
        URL      https://github.com/googleapis/google-cloud-cpp/archive/refs/tags/v2.45.0.tar.gz
        URL_HASH SHA256=3d1b5eb696832f9071bf7ef0b3f0c9fd27c1a39d5edcb8a9976c65193319fd01
        PATCH_COMMAND "${PC}"
        SYSTEM)
if (WIN32)
    add_compile_definitions(_SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING CURL_STATICLIB)
endif()
FetchContent_MakeAvailable(google-cloud-cpp)
