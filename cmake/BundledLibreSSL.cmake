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

function(use_libre_ssl SOURCE_DIR BINARY_DIR)
    message("Using bundled LibreSSL")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT_PREFIX "" CACHE STRING "" FORCE)
        set(BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
        set(BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
    endif()

    set(BYPRODUCTS
            "lib/${BYPRODUCT_PREFIX}tls${BYPRODUCT_SUFFIX}"
            "lib/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}"
            "lib/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}"
            )

    set(LIBRESSL_BIN_DIR "${BINARY_DIR}/thirdparty/libressl-install" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND LIBRESSL_LIBRARIES_LIST "${LIBRESSL_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(LIBRESSL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${LIBRESSL_BIN_DIR}"
            -DLIBRESSL_APPS=OFF
            -DLIBRESSL_TESTS=OFF
            )

    # Build project
    ExternalProject_Add(
        libressl-portable
        URL https://cdn.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.0.2.tar.gz https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.0.2.tar.gz https://gentoo.osuosl.org/distfiles/libressl-3.0.2.tar.gz
        URL_HASH "SHA256=df7b172bf79b957dd27ef36dcaa1fb162562c0e8999e194aa8c1a3df2f15398e"
        SOURCE_DIR "${BINARY_DIR}/thirdparty/libressl-src"
        CMAKE_ARGS ${LIBRESSL_CMAKE_ARGS}
        BUILD_BYPRODUCTS ${LIBRESSL_LIBRARIES_LIST}
        EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(OPENSSL_FOUND "YES" CACHE STRING "" FORCE)
    set(OPENSSL_INCLUDE_DIR "${LIBRESSL_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(OPENSSL_LIBRARIES ${LIBRESSL_LIBRARIES_LIST} CACHE STRING "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${LIBRESSL_BIN_DIR}/lib/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
    set(OPENSSL_SSL_LIBRARY "${LIBRESSL_BIN_DIR}/lib/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    string(REPLACE ";" "%" OPENSSL_LIBRARIES_EXPORT "${OPENSSL_LIBRARIES}")
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_LIBRARIES=${OPENSSL_LIBRARIES_EXPORT}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

    add_library(OpenSSL::Crypto STATIC IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${LIBRESSL_BIN_DIR}/lib/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::Crypto libressl-portable)

    add_library(OpenSSL::SSL STATIC IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${LIBRESSL_BIN_DIR}/lib/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::SSL libressl-portable)
    set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)

    add_library(LibreSSL::TLS STATIC IMPORTED)
    set_target_properties(LibreSSL::TLS PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(LibreSSL::TLS PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${LIBRESSL_BIN_DIR}/lib/${BYPRODUCT_PREFIX}tls${BYPRODUCT_SUFFIX}")
    add_dependencies(LibreSSL::TLS libressl-portable)
    set_property(TARGET LibreSSL::TLS APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
endfunction(use_libre_ssl) 
