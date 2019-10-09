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

function(use_bundled_mbedtls SOURCE_DIR BINARY_DIR)
    message("Using bundled MbedTLS")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT_PREFIX "" CACHE STRING "" FORCE)
        set(BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
        set(BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
    endif()

    set(BYPRODUCTS
            "lib/${BYPRODUCT_PREFIX}mbedtls${BYPRODUCT_SUFFIX}"
            "lib/${BYPRODUCT_PREFIX}mbedx509${BYPRODUCT_SUFFIX}"
            "lib/${BYPRODUCT_PREFIX}mbedcrypto${BYPRODUCT_SUFFIX}"
            )

    set(MBEDTLS_BIN_DIR "${BINARY_DIR}/thirdparty/mbedtls-install" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND MBEDTLS_LIBRARIES_LIST "${MBEDTLS_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(MBEDTLS_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${MBEDTLS_BIN_DIR}"
            -DENABLE_PROGRAMS=OFF
            -DENABLE_TESTING=OFF
            )

    # Build project
    ExternalProject_Add(
            mbedtls-external
            URL "https://tls.mbed.org/download/mbedtls-2.16.3-apache.tgz"
            URL_HASH "SHA256=ec1bee6d82090ed6ea2690784ea4b294ab576a65d428da9fe8750f932d2da661"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/mbedtls-src"
            CMAKE_ARGS ${MBEDTLS_CMAKE_ARGS}
            BUILD_BYPRODUCTS ${MBEDTLS_LIBRARIES_LIST}
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(MBEDTLS_FOUND "YES" CACHE STRING "" FORCE)
    set(MBEDTLS_INCLUDE_DIRS "${MBEDTLS_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(MBEDTLS_LIBRARIES ${MBEDTLS_LIBRARIES_LIST} CACHE STRING "" FORCE)
    set(MBEDTLS_LIBRARY "${MBEDTLS_BIN_DIR}/lib/${BYPRODUCT_PREFIX}mbedtls${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
    set(MBEDX509_LIBRARY "${MBEDTLS_BIN_DIR}/lib/${BYPRODUCT_PREFIX}mbedx509${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
    set(MBEDCRYPTO_LIBRARY "${MBEDTLS_BIN_DIR}/lib/${BYPRODUCT_PREFIX}mbedcrypto${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_INCLUDE_DIRS=${MBEDTLS_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    string(REPLACE ";" "%" MBEDTLS_LIBRARIES_EXPORT "${MBEDTLS_LIBRARIES}")
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_LIBRARIES=${MBEDTLS_LIBRARIES_EXPORT}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_LIBRARY=${MBEDTLS_LIBRARY}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDX509_LIBRARY=${MBEDX509_LIBRARY}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDCRYPTO_LIBRARY=${MBEDCRYPTO_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${MBEDTLS_INCLUDE_DIRS})

    add_library(mbedTLS::mbedcrypto STATIC IMPORTED)
    set_target_properties(mbedTLS::mbedcrypto PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIRS}")
    set_target_properties(mbedTLS::mbedcrypto PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${MBEDCRYPTO_LIBRARY}")
    add_dependencies(mbedTLS::mbedcrypto mbedtls-external)

    add_library(mbedTLS::mbedx509 STATIC IMPORTED)
    set_target_properties(mbedTLS::mbedx509 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIRS}")
    set_target_properties(mbedTLS::mbedx509 PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${MBEDX509_LIBRARY}")
    add_dependencies(mbedTLS::mbedx509 mbedtls-external)
    set_property(TARGET mbedTLS::mbedx509 APPEND PROPERTY INTERFACE_LINK_LIBRARIES mbedTLS::mbedcrypto)

    add_library(mbedTLS::mbedtls STATIC IMPORTED)
    set_target_properties(mbedTLS::mbedtls PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MBEDTLS_INCLUDE_DIRS}")
    set_target_properties(mbedTLS::mbedtls PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${MBEDTLS_LIBRARY}")
    add_dependencies(mbedTLS::mbedtls mbedtls-external)
    set_property(TARGET mbedTLS::mbedtls APPEND PROPERTY INTERFACE_LINK_LIBRARIES mbedTLS::mbedx509 mbedTLS::mbedcrypto)
endfunction(use_bundled_mbedtls)