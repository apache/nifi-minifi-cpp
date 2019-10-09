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

function(use_bundled_libarchive SOURCE_DIR BINARY_DIR)
    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/libarchive.lib")
    else()
        set(BYPRODUCT "lib/libarchive.a")
    endif()

    # Set build options
    set(LIBARCHIVE_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libarchive-install"
            -DENABLE_NETTLE=FALSE
            -DENABLE_OPENSSL=FALSE
            -DENABLE_TAR=FALSE
            -DENABLE_CPIO=FALSE
            -DENABLE_TEST=FALSE)

    # Build project
    ExternalProject_Add(
            libarchive-external
            URL "https://www.libarchive.org/downloads/libarchive-3.3.2.tar.gz"
            URL_HASH "SHA256=ed2dbd6954792b2c054ccf8ec4b330a54b85904a80cef477a1c74643ddafa0ce"
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/libarchive-src"
            CMAKE_ARGS ${LIBARCHIVE_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(LIBARCHIVE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBARCHIVE_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libarchive-install/include" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARY "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARIES ${LIBARCHIVE_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(libarchive STATIC IMPORTED)
    set_target_properties(libarchive PROPERTIES IMPORTED_LOCATION "${LIBARCHIVE_LIBRARY}")
    add_dependencies(libarchive libarchive-external)
    file(MAKE_DIRECTORY ${LIBARCHIVE_INCLUDE_DIR})
    set_property(TARGET libarchive APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBARCHIVE_INCLUDE_DIR})
endfunction(use_bundled_libarchive)
