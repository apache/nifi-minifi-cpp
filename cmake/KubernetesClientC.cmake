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

set(BUILD_TESTING OFF     CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yaml
        GIT_REPOSITORY  https://github.com/yaml/libyaml.git
        GIT_TAG         2c891fc7a770e8ba2fec34fc6b545c672beb37e6  # 0.2.5
)

set(LWS_WITHOUT_TESTAPPS ON             CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER ON          CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER_EXTPOLL ON  CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING ON            CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT ON          CACHE BOOL "" FORCE)
set(LWS_WITH_SHARED OFF                 CACHE BOOL "" FORCE)
set(CMAKE_C_FLAGS "-fpic"               CACHE STRING "" FORCE)
FetchContent_Declare(websockets
        GIT_REPOSITORY  https://libwebsockets.org/repo/libwebsockets.git
        GIT_TAG         43a1e83394bf06689b966402e7d4378685d05fbc  # v4.2-stable
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/kubernetes-client-c/remove-findpackage.patch")
set(PC ${Bash_EXECUTABLE} -c "set -x &&\
        (${Patch_EXECUTABLE} -R -p1 -s -f --dry-run -i ${PATCH_FILE} || ${Patch_EXECUTABLE} -p1 -i ${PATCH_FILE})")
FetchContent_Declare(kubernetes
    GIT_REPOSITORY https://github.com/kubernetes-client/c
    GIT_TAG 9581cd9a8426a5ad7d543b146d5c5ede37cc32e0  # latest commit on master as of 2022-01-05
    SOURCE_SUBDIR kubernetes
    PATCH_COMMAND "${PC}"
)

FetchContent_MakeAvailable(yaml websockets kubernetes)

add_dependencies(websockets CURL::libcurl OpenSSL::Crypto OpenSSL::SSL)
