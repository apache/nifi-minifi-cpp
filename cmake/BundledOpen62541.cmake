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

function(use_bundled_open62541 SOURCE_DIR BINARY_DIR)
    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/open62541/open62541.patch")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/open62541.lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
        set(BYPRODUCT "${LIBDIR}/libopen62541.a")
    endif()

    # Set build options
    set(OPEN62541_BYPRODUCT_DIR "${BINARY_DIR}/thirdparty/open62541-install")

    set(OPEN62541_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${OPEN62541_BYPRODUCT_DIR}"
            -DOPEN62541_VERSION=v1.3.3
            -DUA_ENABLE_ENCRYPTION=ON
            -DUA_FORCE_WERROR=OFF
            -DUA_ENABLE_DEBUG_SANITIZER=OFF)

    append_third_party_passthrough_args(OPEN62541_CMAKE_ARGS "${OPEN62541_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            open62541-external
            URL "https://github.com/open62541/open62541/archive/refs/tags/v1.4.10.tar.gz"
            URL_HASH "SHA256=1a2e762e50bb6dae8d80029dfb66fdbc432876a004e62d618f7cf1bb5b4f495f"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/open62541-src"
            PATCH_COMMAND ${PC}
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${OPEN62541_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${OPEN62541_BYPRODUCT_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    # Set dependencies
    add_dependencies(open62541-external mbedTLS::mbedtls)

    # Set variables
    set(OPEN62541_FOUND "YES" CACHE STRING "" FORCE)
    set(OPEN62541_INCLUDE_DIR "${OPEN62541_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
    set(OPEN62541_LIBRARY "${OPEN62541_BYPRODUCT_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(open62541::open62541 STATIC IMPORTED)
    set_target_properties(open62541::open62541 PROPERTIES IMPORTED_LOCATION "${OPEN62541_LIBRARY}")
    add_dependencies(open62541::open62541 open62541-external)
    file(MAKE_DIRECTORY ${OPEN62541_INCLUDE_DIR})
    set_property(TARGET open62541::open62541 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPEN62541_INCLUDE_DIR})
    set_property(TARGET open62541::open62541 APPEND PROPERTY INTERFACE_LINK_LIBRARIES mbedTLS::mbedtls Threads::Threads)
    if(WIN32)
        set_property(TARGET open62541::open62541 APPEND PROPERTY INTERFACE_LINK_LIBRARIES Ws2_32.lib Iphlpapi.lib)
    endif()
endfunction(use_bundled_open62541)
