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

set(LWS_WITHOUT_TESTAPPS ON                           CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER ON                        CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER_EXTPOLL ON                CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING ON                          CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT ON                        CACHE BOOL "" FORCE)
set(LWS_WITH_SHARED OFF                               CACHE BOOL "" FORCE)
set(LWS_OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}" CACHE STRING "" FORCE)
set(LWS_OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES}"      CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS "-fpic"                             CACHE STRING "" FORCE)

set(WEBSOCKETS_PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/libwebsockets/fix-include-dirs.patch")
set(WEBSOCKETS_PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/libwebsockets/openssl3.patch")
set(WEBSOCKETS_PC ${Bash_EXECUTABLE} -c "set -x &&\
        (${Patch_EXECUTABLE} -R -p1 -s -f --dry-run -i ${WEBSOCKETS_PATCH_FILE_1} || ${Patch_EXECUTABLE} -p1 -i ${WEBSOCKETS_PATCH_FILE_1}) &&\
        (${Patch_EXECUTABLE} -R -p1 -s -f --dry-run -i ${WEBSOCKETS_PATCH_FILE_2} || ${Patch_EXECUTABLE} -p1 -i ${WEBSOCKETS_PATCH_FILE_2}) ")
FetchContent_Declare(websockets
        URL             https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.2.tar.gz
        URL_HASH        SHA256=6a85a1bccf25acc7e8e5383e4934c9b32a102880d1e4c37c70b27ae2a42406e1
        PATCH_COMMAND "${WEBSOCKETS_PC}"
)

FetchContent_MakeAvailable(yaml websockets)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(K8S_PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/kubernetes-client-c/remove-findpackage.patch")
set(K8S_PC ${Bash_EXECUTABLE} -c "set -x &&\
        (${Patch_EXECUTABLE} -R -p1 -s -f --dry-run -i ${K8S_PATCH_FILE} || ${Patch_EXECUTABLE} -p1 -i ${K8S_PATCH_FILE})")
FetchContent_Declare(kubernetes
    URL             https://github.com/kubernetes-client/c/archive/refs/tags/v0.5.0.tar.gz
    URL_HASH        SHA256=dbb6e6cd29ae2ac6c15de894aefb9b1e3d48916541d443f089aa0ffad6517ec6
    PATCH_COMMAND "${K8S_PC}"
)

# With CMake >= 3.18, this block could be replaced with FetchContent_MakeAvailable(kubernetes),
# if we add the `SOURCE_SUBDIR kubernetes` option to FetchContent_Declare() [this option is not available in CMake < 3.18].
# As of July 2022, one of our supported platforms, Centos 7, comes with CMake 3.17.
FetchContent_GetProperties(kubernetes)
if(NOT kubernetes_POPULATED)
    FetchContent_Populate(kubernetes)
    # the top level doesn't contain CMakeLists.txt, it is in the "kubernetes" subdirectory
    add_subdirectory(${kubernetes_SOURCE_DIR}/kubernetes ${kubernetes_BINARY_DIR})
endif()

add_dependencies(websockets CURL::libcurl OpenSSL::Crypto OpenSSL::SSL)
