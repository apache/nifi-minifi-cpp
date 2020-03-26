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

function(use_bundled_bzip2 SOURCE_DIR BINARY_DIR)
    message("Using bundled bzip2")

    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/bzip2/bzip2.patch")

    # Define byproduct
    if (WIN32)
        set(BYPRODUCT "lib/bz2.lib")
    else()
        set(BYPRODUCT "lib/libbz2.a")
    endif()

    # Set build options
    set(BZIP2_BIN_DIR "${BINARY_DIR}/thirdparty/bzip2-install" CACHE STRING "" FORCE)

    set(BZIP2_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BZIP2_BIN_DIR}")

    # Build project
    ExternalProject_Add(
            bzip2-external
            URL https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
            URL_HASH "SHA256=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/bzip2-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${BZIP2_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BZIP2_BIN_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(BZIP2_FOUND "YES" CACHE STRING "" FORCE)
    set(BZIP2_INCLUDE_DIRS "${BZIP2_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(BZIP2_LIBRARIES "${BZIP2_BIN_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_BZIP2_INCLUDE_DIRS=${BZIP2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_BZIP2_LIBRARIES=${BZIP2_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${BZIP2_INCLUDE_DIRS})

    add_library(BZip2::BZip2 STATIC IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES IMPORTED_LOCATION "${BZIP2_LIBRARIES}")
    add_dependencies(BZip2::BZip2 bzip2-external)
    set_property(TARGET BZip2::BZip2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIRS}")
endfunction(use_bundled_bzip2)
