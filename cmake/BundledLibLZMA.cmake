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

function(use_bundled_liblzma SOURCE_DIR BINARY_DIR)
    message("Using bundled liblzma")

    # Define patch step
    if (WIN32)
        set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/liblzma/liblzma.patch")
    endif()

    # Define byproduct
    if (WIN32)
        set(BYPRODUCT "lib/lzma.lib")
    else()
        set(BYPRODUCT "lib/liblzma.a")
    endif()

    # Set build options
    set(LIBLZMA_BIN_DIR "${BINARY_DIR}/thirdparty/liblzma-install" CACHE STRING "" FORCE)

    if (WIN32)
        set(LIBLZMA_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${LIBLZMA_BIN_DIR}")
    endif()

    # Build project
    set(LIBLZMA_URL https://tukaani.org/xz/xz-5.2.5.tar.gz https://gentoo.osuosl.org/distfiles/xz-5.2.5.tar.gz)
    set(LIBLZMA_URL_HASH "SHA256=f6f4910fd033078738bd82bfba4f49219d03b17eb0794eb91efbae419f4aba10")

    if (WIN32)
        ExternalProject_Add(
                liblzma-external
                URL ${LIBLZMA_URL}
                URL_HASH ${LIBLZMA_URL_HASH}
                SOURCE_DIR "${BINARY_DIR}/thirdparty/liblzma-src"
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                CMAKE_ARGS ${LIBLZMA_CMAKE_ARGS}
                PATCH_COMMAND ${PC}
                BUILD_BYPRODUCTS "${LIBLZMA_BIN_DIR}/${BYPRODUCT}"
                EXCLUDE_FROM_ALL TRUE
        )
    else()
        set(CONFIGURE_COMMAND ./configure --disable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo --disable-lzma-links --disable-scripts --disable-doc --enable-shared=no "--prefix=${LIBLZMA_BIN_DIR}")
        if(PORTABLE)
            list(APPEND CONFIGURE_COMMAND "--disable-assembler")
        endif()

        ExternalProject_Add(
                liblzma-external
                URL ${LIBLZMA_URL}
                URL_HASH ${LIBLZMA_URL_HASH}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/liblzma-src"
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS "${LIBLZMA_BIN_DIR}/${BYPRODUCT}"
                CONFIGURE_COMMAND ""
                PATCH_COMMAND ${CONFIGURE_COMMAND}
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
        )
    endif()

    # Set variables
    set(LIBLZMA_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBLZMA_INCLUDE_DIRS "${LIBLZMA_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(LIBLZMA_LIBRARIES "${LIBLZMA_BIN_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBLZMA_INCLUDE_DIRS=${LIBLZMA_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBLZMA_LIBRARIES=${LIBLZMA_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBLZMA_INCLUDE_DIRS})

    add_library(LibLZMA::LibLZMA STATIC IMPORTED)
    set_target_properties(LibLZMA::LibLZMA PROPERTIES IMPORTED_LOCATION "${LIBLZMA_LIBRARIES}")
    add_dependencies(LibLZMA::LibLZMA liblzma-external)
    set_property(TARGET LibLZMA::LibLZMA APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBLZMA_INCLUDE_DIRS}")
endfunction(use_bundled_liblzma)
