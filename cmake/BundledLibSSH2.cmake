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
    message("Using bundled libssh2")

    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libssh2/libssh2-CMAKE_MODULE_PATH.patch")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/libssh2.lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
        set(BYPRODUCT "${LIBDIR}/libssh2.a")
    endif()

    # Set build options
    set(LIBSSH2_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libssh2-install"
            -DENABLE_ZLIB_COMPRESSION=ON
            -DCRYPTO_BACKEND=OpenSSL
            -DBUILD_TESTING=OFF
            -DBUILD_EXAMPLES=OFF)

    append_third_party_passthrough_args(LIBSSH2_CMAKE_ARGS "${LIBSSH2_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            libssh2-external
            URL "https://github.com/libssh2/libssh2/releases/download/libssh2-1.10.0/libssh2-1.10.0.tar.gz"
            URL_HASH "SHA256=2d64e90f3ded394b91d3a2e774ca203a4179f69aebee03003e5a6fa621e41d51"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libssh2-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBSSH2_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libssh2-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(libssh2-external OpenSSL::Crypto ZLIB::ZLIB)

    # Set variables
    set(LIBSSH2_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBSSH2_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libssh2-install/include" CACHE STRING "" FORCE)
    set(LIBSSH2_LIBRARY "${BINARY_DIR}/thirdparty/libssh2-install/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSSH2_INCLUDE_DIR=${LIBSSH2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSSH2_LIBRARY=${LIBSSH2_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBSSH2_INCLUDE_DIR})

    add_library(libssh2 STATIC IMPORTED)
    set_target_properties(libssh2 PROPERTIES IMPORTED_LOCATION "${LIBSSH2_LIBRARY}")
    add_dependencies(libssh2 libssh2-external)
    set_property(TARGET libssh2 APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto ZLIB::ZLIB)
    set_property(TARGET libssh2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBSSH2_INCLUDE_DIR})
endfunction(use_bundled_libssh2)
