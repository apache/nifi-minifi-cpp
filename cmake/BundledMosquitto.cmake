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

function(use_bundled_mosquitto SOURCE_DIR BINARY_DIR)
    # Define patch step
    # set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/mosquitto/mosquitto.patch")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/mosquitto-static.lib")
    else()
        include(GNUInstallDirs)
        set(BYPRODUCT "${CMAKE_INSTALL_LIBDIR}/libmosquitto-static.a")
    endif()

    # Set build options
    set(MOSQUITTO_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/mosquitto-install"
            -DDOCUMENTATION=OFF
            -DWITH_STATIC_LIBRARIES=ON
            -DWITH_PIC=ON
            -DCMAKE_MACOSX_RPATH=1)
    if (OPENSSL_OFF)
        list(APPEND MOSQUITTO_CMAKE_ARGS -DWITH_TLS=OFF)
    else()
        list(APPEND MOSQUITTO_CMAKE_ARGS -DWITH_TLS=OFF)
    endif()

    append_third_party_passthrough_args(MOSQUITTO_CMAKE_ARGS "${MOSQUITTO_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            mosquitto-external
            GIT_REPOSITORY "https://github.com/eclipse/mosquitto.git"
            GIT_TAG "8123e767dec0dc5ed00446e1127ab1064c6412d6"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/mosquitto-1.6.10-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${MOSQUITTO_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/mosquitto-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    if (NOT OPENSSL_OFF)
        add_dependencies(mosquitto-external OpenSSL::SSL OpenSSL::Crypto)
    endif()

    # Set variables
    set(MOSQUITTO_FOUND "YES" CACHE STRING "" FORCE)
    set(MOSQUITTO_INCLUDE_DIR "${BINARY_DIR}/thirdparty/mosquitto-install/include" CACHE STRING "" FORCE)
    set(MOSQUITTO_LIBRARY "${BINARY_DIR}/thirdparty/mosquitto-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(MOSQUITTO_LIBRARIES ${MOSQUITTO_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(mosquitto STATIC IMPORTED)
    set_target_properties(mosquitto PROPERTIES IMPORTED_LOCATION "${MOSQUITTO_LIBRARY}")
    add_dependencies(mosquitto mosquitto-external)
    file(MAKE_DIRECTORY ${MOSQUITTO_INCLUDE_DIR})
    set_property(TARGET mosquitto APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${MOSQUITTO_INCLUDE_DIR})
    if (NOT OPENSSL_OFF)
        set_property(TARGET mosquitto APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    endif()

endfunction(use_bundled_mosquitto)
