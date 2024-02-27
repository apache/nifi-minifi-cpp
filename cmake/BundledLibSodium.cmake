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

function(use_bundled_libsodium SOURCE_DIR BINARY_DIR)
    message("Using bundled libsodium")

    # Define patch step
    if (WIN32)
        set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libsodium/libsodium.patch")
    endif()

    # Define byproduct
    if (WIN32)
        set(BYPRODUCT "lib/sodium.lib")
    else()
        set(BYPRODUCT "lib/libsodium.a")
    endif()

    # Set build options
    set(LIBSODIUM_BIN_DIR "${BINARY_DIR}/thirdparty/libsodium-install" CACHE STRING "" FORCE)

    if (WIN32)
        set(LIBSODIUM_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${LIBSODIUM_BIN_DIR}"
                "-DSODIUM_LIBRARY_MINIMAL=1")
    endif()

    # Build project
    set(LIBSODIUM_OFFICIAL_MIRROR_URL https://download.libsodium.org/libsodium/releases/libsodium-1.0.18.tar.gz)
    set(LIBSODIUM_GITHUB_MIRROR_URL https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz)
    set(LIBSODIUM_GENTOO_MIRROR_URL https://gentoo.osuosl.org/distfiles/libsodium-1.0.18.tar.gz)
    set(LIBSODIUM_URL_HASH "SHA256=6f504490b342a4f8a4c4a02fc9b866cbef8622d5df4e5452b46be121e46636c1")

    if (WIN32)
        ExternalProject_Add(
                libsodium-external
                URL "${LIBSODIUM_OFFICIAL_MIRROR_URL}" "${LIBSODIUM_GITHUB_MIRROR_URL}" "${LIBSODIUM_GENTOO_MIRROR_URL}"
                URL_HASH ${LIBSODIUM_URL_HASH}
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libsodium-src"
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                CMAKE_ARGS ${LIBSODIUM_CMAKE_ARGS}
                PATCH_COMMAND ${PC}
                BUILD_BYPRODUCTS "${LIBSODIUM_BIN_DIR}/${BYPRODUCT}"
                EXCLUDE_FROM_ALL TRUE
        )
    else()
        set(CONFIGURE_COMMAND ./configure --disable-pie --enable-minimal "--prefix=${LIBSODIUM_BIN_DIR}")

        ExternalProject_Add(
                libsodium-external
                URL "${LIBSODIUM_OFFICIAL_MIRROR_URL}" "${LIBSODIUM_GITHUB_MIRROR_URL}" "${LIBSODIUM_GENTOO_MIRROR_URL}"
                URL_HASH ${LIBSODIUM_URL_HASH}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libsodium-src"
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS "${LIBSODIUM_BIN_DIR}/${BYPRODUCT}"
                CONFIGURE_COMMAND "${CONFIGURE_COMMAND}"
                PATCH_COMMAND ""
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
        )
    endif()

    # Set variables
    set(LIBSODIUM_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBSODIUM_INCLUDE_DIRS "${LIBSODIUM_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(LIBSODIUM_LIBRARIES "${LIBSODIUM_BIN_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSODIUM_INCLUDE_DIRS=${LIBSODIUM_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBSODIUM_LIBRARIES=${LIBSODIUM_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBSODIUM_INCLUDE_DIRS})

    add_library(libsodium STATIC IMPORTED)
    set_target_properties(libsodium PROPERTIES IMPORTED_LOCATION "${LIBSODIUM_LIBRARIES}")
    add_dependencies(libsodium libsodium-external)
    set_property(TARGET libsodium APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBSODIUM_INCLUDE_DIRS}")
endfunction(use_bundled_libsodium)
