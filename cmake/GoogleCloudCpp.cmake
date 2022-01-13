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
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

set(GOOGLE_CLOUD_CPP_ENABLE storage CACHE INTERNAL storage-api)
set(GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK OFF CACHE INTERNAL macos-openssl-check)
set(BUILD_TESTING OFF CACHE INTERNAL testing-off)
set(GOOGLE_CLOUD_CPP_ENABLE_WERROR OFF CACHE INTERNAL warnings-off)
FetchContent_Declare(google-cloud-cpp
        URL      https://github.com/googleapis/google-cloud-cpp/archive/refs/tags/v1.37.0.tar.gz
        URL_HASH SHA256=a7269b21d5e95bebff7833ebb602bcd5bcc79e82a59449cc5d5b350ff2f50bbc
        PATCH_COMMAND "${PC}")
add_compile_definitions(_SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING CURL_STATICLIB)
FetchContent_MakeAvailable(google-cloud-cpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.0.0" )
    target_compile_options(google_cloud_cpp_common PUBLIC -Wno-error=deprecated-pragma)
endif()

if (WIN32)
    target_compile_options(google_cloud_cpp_storage PUBLIC /wd4996)
else()
    target_compile_options(google_cloud_cpp_storage PUBLIC -Wno-error=deprecated-declarations)
endif()
