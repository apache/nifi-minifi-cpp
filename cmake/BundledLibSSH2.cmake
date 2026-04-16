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

function(use_bundled_libssh2 SOURCE_DIR BINARY_DIR)
    message(STATUS "Using bundled libssh2 via FetchContent")

    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)

    include(FetchContent)

    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libssh2/libssh2-CMAKE_MODULE_PATH.patch")

    FetchContent_Declare(
            libssh2
            URL "https://github.com/libssh2/libssh2/releases/download/libssh2-1.10.0/libssh2-1.10.0.tar.gz"
            URL_HASH "SHA256=2d64e90f3ded394b91d3a2e774ca203a4179f69aebee03003e5a6fa621e41d51"
            PATCH_COMMAND ${PC}
            SYSTEM
            OVERRIDE_FIND_PACKAGE
    )

    set(ENABLE_ZLIB_COMPRESSION ON CACHE BOOL "" FORCE)
    set(CRYPTO_BACKEND "OpenSSL" CACHE STRING "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(libssh2)

    target_link_libraries(libssh2 PUBLIC OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
endfunction(use_bundled_libssh2)
