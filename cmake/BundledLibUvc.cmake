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

function(use_bundled_libuvc SOURCE_DIR BINARY_DIR)
    find_package(PkgConfig)
    pkg_check_modules(LIBUSB libusb-1.0)

    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libuvc/libuvc.patch")

    # Define patch step
    if (WIN32)
        set(BYPRODUCT "lib/${CMAKE_LIBRARY_ARCHITECTURE}/libuvc.lib")
    else()
        set(BYPRODUCT "lib/${CMAKE_LIBRARY_ARCHITECTURE}/libuvc.a")
    endif()

    # Set build options
    set(LIBUVC_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libuvc-install")

    # Build project
    ExternalProject_Add(
            libuvc-external
            GIT_REPOSITORY "https://github.com/libuvc/libuvc.git"
            GIT_TAG "v0.0.6"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libuvc-src"
            CMAKE_ARGS ${LIBUVC_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libuvc-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(LIBUVC_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBUVC_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libuvc-install/include" CACHE STRING "" FORCE)
    set(LIBUVC_LIBRARY "${BINARY_DIR}/thirdparty/libuvc-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBUVC_LIBRARIES ${LIBUVC_LIBRARY} CACHE STRING "" FORCE)

    add_library(libuvc STATIC IMPORTED)
    set_target_properties(libuvc PROPERTIES IMPORTED_LOCATION "${LIBUVC_LIBRARY}")
    add_dependencies(libuvc libuvc-external)
    file(MAKE_DIRECTORY ${LIBUVC_INCLUDE_DIR})
    set_property(TARGET libuvc APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBUVC_INCLUDE_DIR})
    set_property(TARGET libuvc APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${LIBUSB_LIBRARIES})
endfunction(use_bundled_libuvc)
