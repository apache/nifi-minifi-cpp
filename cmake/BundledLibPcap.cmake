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

function(use_bundled_libpcap SOURCE_DIR BINARY_DIR)
    message("Using bundled libpcap")

    if(USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt libpcap external lib")
        include(${CMAKE_BINARY_DIR}/libpcap-config.cmake)

    elseif(USE_CMAKE_FETCH_CONTENT)
        message("Using CMAKE's ExternalProject_Add to manage source building libpcap external lib")

        # Define byproducts
        if(WIN32)
            set(LIBPCAP_BYPRODUCT "lib/libpcap.lib")
        else()
            # set(LIBPCAP_BYPRODUCT "lib/libpcap.a")
            set(LIBPCAP_BYPRODUCT "lib/libpcap.so")
        endif()

        set(LIBPCAP_BYPRODUCT_DIR "${BINARY_DIR}/thirdparty/libpcap-install")

        # Build project
        set(LIBPCAP_URL https://github.com/the-tcpdump-group/libpcap/archive/libpcap-1.10.1.tar.gz)
        set(LIBPCAP_URL_HASH "SHA256=7b650c9e0ce246aa41ba5463fe8e903efc444c914a3ccb986547350bed077ed6")

        set(LIBPCAP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS} 
            "-DCMAKE_INSTALL_PREFIX=${LIBPCAP_BYPRODUCT_DIR}"
            "-DBUILD_SHARED_LIBS=ON")

        append_third_party_passthrough_args(LIBPCAP_CMAKE_ARGS "${LIBPCAP_CMAKE_ARGS}")

        ExternalProject_Add(
                libpcap-external
                URL ${LIBPCAP_URL}
                URL_HASH ${LIBPCAP_URL_HASH}
                CMAKE_ARGS ${LIBPCAP_CMAKE_ARGS}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/libpcap-src"
                STEP_TARGETS build
                BUILD_BYPRODUCTS "${LIBPCAP_BYPRODUCT_DIR}/${LIBPCAP_BYPRODUCT}"
                EXCLUDE_FROM_ALL TRUE
        )

        # Set variables
        set(PCAP_FOUND "YES" CACHE STRING "" FORCE)
        set(PCAP_ROOT_DIR "${LIBPCAP_BYPRODUCT_DIR}" CACHE STRING "" FORCE)
        set(PCAP_INCLUDE_DIR "${LIBPCAP_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
        set(PCAP_LIBRARIES "${LIBPCAP_BYPRODUCT_DIR}/${LIBPCAP_BYPRODUCT}" CACHE STRING "" FORCE)
        set(PCAP_LIBRARY "${PCAP_LIBRARIES}" CACHE STRING "" FORCE)

        # Set exported variables for FindPackage.cmake
        set(EXPORTED_PCAP_INCLUDE_DIRS "${PCAP_INCLUDE_DIR}" CACHE STRING "" FORCE)
        set(EXPORTED_PCAP_LIBRARIES "${PCAP_LIBRARY}" CACHE STRING "" FORCE)

        # Create imported targets
        file(MAKE_DIRECTORY ${PCAP_INCLUDE_DIR})

        add_library(PCAP SHARED IMPORTED)
        add_library(PCAP::PCAP ALIAS PCAP)
        set_target_properties(PCAP PROPERTIES IMPORTED_LOCATION "${PCAP_LIBRARY}")
        add_dependencies(PCAP libpcap-external)
        set_property(TARGET PCAP APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${PCAP_INCLUDE_DIR})

    endif()
endfunction(use_bundled_libpcap)
