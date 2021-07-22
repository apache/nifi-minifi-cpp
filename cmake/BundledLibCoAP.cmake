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

function(use_bundled_libcoap SOURCE_DIR BINARY_DIR)
    message("Using bundled libcoap")

    # Define patch step
    if (WIN32)
        set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libcoap/libcoap-windows-cmake.patch")
    endif()

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/coap.lib")
    else()
        set(BYPRODUCT "lib/libcoap-2.a")
    endif()

    # Build project
    set(LIBCOAP_URL https://github.com/obgm/libcoap/archive/v4.2.1.tar.gz)
    set(LIBCOAP_URL_HASH "SHA256=29a0394a265d3febee41e5e2dc03d34292a0aede37f5f80334e529ac0dab2321")

    if (WIN32)
        set(LIBCOAP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libcoap-install")

        ExternalProject_Add(
                coap-external
                URL ${LIBCOAP_URL}
                URL_HASH ${LIBCOAP_URL_HASH}
                CMAKE_ARGS ${LIBCOAP_CMAKE_ARGS}
                PATCH_COMMAND ${PC}
                BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libcoap-install/${BYPRODUCT}"
                EXCLUDE_FROM_ALL TRUE
        )
    else()
        ExternalProject_Add(
                coap-external
                URL ${LIBCOAP_URL}
                URL_HASH ${LIBCOAP_URL_HASH}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libcoap-src"
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libcoap-install/${BYPRODUCT}"
                CONFIGURE_COMMAND ""
                PATCH_COMMAND ./autogen.sh && ./configure "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" --with-pic --disable-examples --disable-dtls --disable-tests --disable-documentation --prefix=${BINARY_DIR}/thirdparty/libcoap-install
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
        )
    endif()

    # Set variables
    set(COAP_FOUND "YES" CACHE STRING "" FORCE)
    set(COAP_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/libcoap-install/include" CACHE STRING "" FORCE)
    set(COAP_LIBRARY "${BINARY_DIR}/thirdparty/libcoap-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(COAP_LIBRARIES "${COAP_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${COAP_INCLUDE_DIRS})

    add_library(COAP::libcoap STATIC IMPORTED)
    set_target_properties(COAP::libcoap PROPERTIES IMPORTED_LOCATION "${COAP_LIBRARY}")
    add_dependencies(COAP::libcoap coap-external)
    set_property(TARGET COAP::libcoap APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${COAP_INCLUDE_DIRS}")
    set_property(TARGET COAP::libcoap APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
endfunction(use_bundled_libcoap)
