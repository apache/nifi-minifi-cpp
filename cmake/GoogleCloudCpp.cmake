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
set(PATCH_FILE_3 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/mock-client-without-decorators.patch")
set(PATCH_FILE_4 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/mock_client_target.patch")
set(PATCH_FILE_5 "${CMAKE_SOURCE_DIR}/thirdparty/google-cloud-cpp/c++23_fixes.patch")
if (SKIP_TESTS)
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_5}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_5}\\\")")
else()
    include(GoogleTest)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL            https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz
            URL_HASH       SHA256=b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5
    )
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_3}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_4}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_4}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_5}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_5}\\\")")
endif()

set(GOOGLE_CLOUD_CPP_ENABLE storage CACHE INTERNAL storage-api)
set(GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK OFF CACHE INTERNAL macos-openssl-check)
set(BUILD_TESTING OFF CACHE INTERNAL testing-off)
set(GOOGLE_CLOUD_CPP_ENABLE_WERROR OFF CACHE INTERNAL warnings-off)
FetchContent_Declare(google-cloud-cpp
        URL      https://github.com/googleapis/google-cloud-cpp/archive/refs/tags/v2.10.1.tar.gz
        URL_HASH SHA256=dbd8ff9cdda8b991049094c41d9125438098422658c77dc1afab524c589efeda
        PATCH_COMMAND "${PC}")
add_compile_definitions(_SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING CURL_STATICLIB)
FetchContent_MakeAvailable(google-cloud-cpp)

if (NOT SKIP_TESTS)
    get_target_property(GMOCK_INC_DIR mock_google_cloud_client INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(mock_google_cloud_client PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${GMOCK_INC_DIR}")
endif()
