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
if(TARGET civetweb::civetweb-cpp)
    return()
endif()

include(FetchContent)

set(CIVETWEB_ENABLE_SSL_DYNAMIC_LOADING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_BUILD_TESTING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_DUKTAPE "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_LUA "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_CXX "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ALLOW_WARNINGS "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_ASAN "OFF" CACHE STRING "" FORCE)
set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/civetweb/openssl3.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(civetweb
    URL      https://github.com/civetweb/civetweb/archive/refs/tags/v1.16.tar.gz
    URL_HASH SHA256=f0e471c1bf4e7804a6cfb41ea9d13e7d623b2bcc7bc1e2a4dd54951a24d60285
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(civetweb)

add_dependencies(civetweb-c-library OpenSSL::Crypto OpenSSL::SSL)
add_dependencies(civetweb-cpp OpenSSL::Crypto OpenSSL::SSL)

target_compile_definitions(civetweb-c-library PRIVATE SOCKET_TIMEOUT_QUANTUM=200)

add_library(civetweb::c-library ALIAS civetweb-c-library)
add_library(civetweb::civetweb-cpp ALIAS civetweb-cpp)

set(CIVETWEB_INCLUDE_DIR "${civetweb_SOURCE_DIR}/include")
set(CIVETWEB_INCLUDE_DIRS "${CIVETWEB_INCLUDE_DIR}")
