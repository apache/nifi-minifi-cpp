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

function(use_bundled_zlib SOURCE_DIR BINARY_DIR)
    message("Using bundled zlib")

    # Define byproducts
    if (WIN32)
        string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
        if (build_type MATCHES relwithdebinfo OR build_type MATCHES release)
            set(BYPRODUCT "lib/zlibstatic.lib")
        else()
            set(BYPRODUCT "lib/zlibstaticd.lib")
        endif()
    else()
        set(BYPRODUCT "lib/libz.a")
    endif()

    # Set build options
    set(ZLIB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/zlib-install"
            )

    # Build project
    ExternalProject_Add(
        zlib-external
        URL "https://github.com/madler/zlib/archive/v1.2.11.tar.gz"
        URL_HASH "SHA256=629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff"
        SOURCE_DIR "${BINARY_DIR}/thirdparty/zlib-src"
        CMAKE_ARGS ${ZLIB_CMAKE_ARGS}
        BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/zlib-install/${BYPRODUCT}"
        EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(ZLIB_FOUND "YES" CACHE STRING "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/zlib-install/include" CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES "${BINARY_DIR}/thirdparty/zlib-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(ZLIB_VERSION_STRING "1.2.11" CACHE STRING "" FORCE)
    set(ZLIB_VERSION_MAJOR 1 CACHE STRING "" FORCE)
    set(ZLIB_VERSION_MINOR 2 CACHE STRING "" FORCE)
    set(ZLIB_VERSION_PATCH 11 CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_INCLUDE_DIRS=${ZLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_LIBRARIES=${ZLIB_LIBRARIES}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_VERSION_STRING=${ZLIB_VERSION_STRING}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_VERSION_MAJOR=${ZLIB_VERSION_MAJOR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_VERSION_MINOR=${ZLIB_VERSION_MINOR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_VERSION_PATCH=${ZLIB_VERSION_PATCH}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${ZLIB_INCLUDE_DIRS})

    add_library(ZLIB::ZLIB STATIC IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES IMPORTED_LOCATION "${ZLIB_LIBRARIES}")
    add_dependencies(ZLIB::ZLIB zlib-external)
    file(MAKE_DIRECTORY ${ZLIB_INCLUDE_DIRS})
    set_property(TARGET ZLIB::ZLIB APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIRS})
endfunction(use_bundled_zlib)
