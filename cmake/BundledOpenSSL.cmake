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

function(use_openssl SOURCE_DIR BINARY_DIR)
    message("Using bundled OpenSSL")

    if(APPLE OR WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LIBDIR "lib")
    else()
        set(LIBDIR "lib64")
    endif()

    # Define byproducts
    set(BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
    if (WIN32)
        set(BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
    elseif (APPLE AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"))
        set(BYPRODUCT_SUFFIX ".dylib" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
    endif()

    if (APPLE AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"))
        set(OPENSSL_SHARED_FLAG "" CACHE STRING "" FORCE)
    else()
        set(OPENSSL_SHARED_FLAG "no-shared" CACHE STRING "" FORCE)
    endif()

    set(BYPRODUCTS
            "${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}"
            "${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}"
            )

    set(OPENSSL_BIN_DIR "${BINARY_DIR}/thirdparty/openssl-install" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND OPENSSL_LIBRARIES_LIST "${OPENSSL_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(OPENSSL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${OPENSSL_BIN_DIR}"
            "-DCMAKE_POLICY_DEFAULT_CMP0063=NEW"
            # avoid polluting the global namespace, otherwise could interfere with the system openssl (e.g. python script extension using numpy)
            "-DCMAKE_C_VISIBILITY_PRESET=hidden"
            "-DCMAKE_CXX_VISIBILITY_PRESET=hidden"
            "-DCMAKE_VISIBILITY_INLINES_HIDDEN=ON"
            )

    if (WIN32)
        ExternalProject_Add(
                openssl-external
                URL https://github.com/openssl/openssl/releases/download/openssl-3.1.0/openssl-3.1.0.tar.gz
                URL_HASH "SHA256=aaa925ad9828745c4cad9d9efeb273deca820f2cdcf2c3ac7d7c1212b7c497b4"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND perl Configure "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" ${OPENSSL_SHARED_FLAG} "--prefix=${OPENSSL_BIN_DIR}" "--openssldir=${OPENSSL_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_LIBRARIES_LIST}
                EXCLUDE_FROM_ALL TRUE
                BUILD_COMMAND nmake
                INSTALL_COMMAND nmake install
            )
    else()
        ExternalProject_Add(
                openssl-external
                URL https://github.com/openssl/openssl/releases/download/openssl-3.1.0/openssl-3.1.0.tar.gz
                URL_HASH "SHA256=aaa925ad9828745c4cad9d9efeb273deca820f2cdcf2c3ac7d7c1212b7c497b4"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND ./Configure "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" ${OPENSSL_SHARED_FLAG} "--prefix=${OPENSSL_BIN_DIR}" "--openssldir=${OPENSSL_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_LIBRARIES_LIST}
                EXCLUDE_FROM_ALL TRUE
        )
    endif()

    # Set variables
    set(OPENSSL_FOUND "YES" CACHE STRING "" FORCE)
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES_LIST};${CMAKE_DL_LIBS}"  CACHE STRING "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)

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
            IMPORTED_LOCATION "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::Crypto openssl-external)

    add_library(OpenSSL::SSL STATIC IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::SSL openssl-external)
    set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)

    if(WIN32)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib )
        set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
    endif()
endfunction(use_openssl)
