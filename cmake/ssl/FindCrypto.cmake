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

if(TARGET crypto OR TARGET AWS::crypto)
    set(CRYPTO_FOUND TRUE)
    set(crypto_FOUND TRUE)
    return()
endif()

if(NOT OPENSSL_ROOT_DIR)
    message(FATAL_ERROR "Strict bundled crypto requires OPENSSL_ROOT_DIR to be passed to this CMake scope!")
endif()

find_library(crypto_STATIC_LIBRARY
        NAMES libcrypto.a crypto.lib
        PATHS "${OPENSSL_ROOT_DIR}/lib" "${OPENSSL_ROOT_DIR}/lib64"
        NO_DEFAULT_PATH # Strictly prevent system fallback
)

set(crypto_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
set(crypto_LIBRARY "${crypto_STATIC_LIBRARY}")

if(NOT crypto_LIBRARY OR NOT EXISTS "${crypto_INCLUDE_DIR}")
    message(FATAL_ERROR "Failed to locate bundled crypto (libcrypto) inside ${OPENSSL_ROOT_DIR}")
endif()

set(CRYPTO_FOUND TRUE)
set(crypto_FOUND TRUE)
set(crypto_SHARED_LIBRARY "")
set(crypto_STATIC_LIBRARY "${crypto_LIBRARY}")

add_library(AWS::crypto UNKNOWN IMPORTED)
set_target_properties(AWS::crypto PROPERTIES
        IMPORTED_LOCATION "${crypto_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${crypto_INCLUDE_DIR}"
)

add_library(crypto ALIAS AWS::crypto)

if(WIN32)
    set_property(TARGET AWS::crypto APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
endif()
