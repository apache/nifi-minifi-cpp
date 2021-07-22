#
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
#

function(use_bundled_libxml2 SOURCE_DIR BINARY_DIR)
    message("Using bundled libxml2")

    # Define patch step
    if (WIN32)
        set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libxml2/libxml2-win.patch")
    endif()

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/xml2.lib")
    else()
        set(BYPRODUCT "lib/libxml2.a")
    endif()

    # Set build options
    if (WIN32)
        set(LIBXML2_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libxml2-install")
    endif()

    # Build project
    set(LIBXML2_URL ftp://xmlsoft.org/libxml2/libxml2-2.9.10.tar.gz https://ftp.osuosl.org/pub/blfs/conglomeration/libxml2/libxml2-2.9.10.tar.gz)
    set(LIBXML2_URL_HASH "SHA256=aafee193ffb8fe0c82d4afef6ef91972cbaf5feea100edc2f262750611b4be1f")

    if (WIN32)
        ExternalProject_Add(
                libxml2-external
                URL ${LIBXML2_URL}
                URL_HASH ${LIBXML2_URL_HASH}
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libxml2-src"
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                CMAKE_ARGS ${LIBXML2_CMAKE_ARGS}
                PATCH_COMMAND ${PC}
                BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libxml2-install/${BYPRODUCT}"
                EXCLUDE_FROM_ALL TRUE
        )
    else()
        ExternalProject_Add(
                libxml2-external
                URL ${LIBXML2_URL}
                URL_HASH ${LIBXML2_URL_HASH}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libxml2-src"
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libxml2-install/${BYPRODUCT}"
                CONFIGURE_COMMAND ""
                PATCH_COMMAND ./configure --enable-shared=no --enable-static=yes --with-pic=yes --with-iconv=no --with-zlib=no --with-lzma=no --with-python=no --with-ftp=no --with-http=no --prefix=${BINARY_DIR}/thirdparty/libxml2-install
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
        )
    endif()

    # Set variables
    set(LIBXML2_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libxml2-install/include/libxml2" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIRS "${LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARY "${BINARY_DIR}/thirdparty/libxml2-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARIES "${LIBXML2_LIBRARY}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBXML2_INCLUDE_DIR=${LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBXML2_LIBRARY=${LIBXML2_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBXML2_INCLUDE_DIR})

    add_library(LibXml2::LibXml2 STATIC IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES IMPORTED_LOCATION "${LIBXML2_LIBRARY}")
    add_dependencies(LibXml2::LibXml2 libxml2-external)
    set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}")
endfunction(use_bundled_libxml2)
