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


if (NOT OPENSSL_ROOT_DIR)
    message(FATAL_ERROR "Strict bundled OpenSSL requires OPENSSL_ROOT_DIR to be passed to this CMake scope!")
endif ()

if (WIN32)
    set(LIB_EXT ".lib")
elseif (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    set(LIB_EXT ".dylib")
else ()
    set(LIB_EXT ".a")
endif ()

if (APPLE OR WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 4 OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|armv8)$")
    set(OSSL_LIBDIR "lib")
else ()
    set(OSSL_LIBDIR "lib64")
endif ()

set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE STRING "" FORCE)
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/${OSSL_LIBDIR}/libcrypto${LIB_EXT}" CACHE STRING "" FORCE)
set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/${OSSL_LIBDIR}/libssl${LIB_EXT}" CACHE STRING "" FORCE)
set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} CACHE STRING "" FORCE)

set(OPENSSL_FOUND TRUE CACHE STRING "" FORCE)
set(OpenSSL_FOUND TRUE CACHE STRING "" FORCE)

if (TARGET OpenSSL::Crypto AND TARGET OpenSSL::SSL)
    set(OPENSSL_FOUND TRUE)
    set(OpenSSL_FOUND TRUE)
    return()
endif ()

add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")

add_library(OpenSSL::SSL UNKNOWN IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")

if (WIN32)
    set_property(TARGET OpenSSL::Crypto APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
    set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
endif ()
