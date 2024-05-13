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
function(use_bundled_pcapplusplus SOURCE_DIR BINARY_DIR)
    message("Using bundled pcapplusplus")

    if(USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt PcapPlusPlus external lib")
        include(${CMAKE_BINARY_DIR}/pcapplusplus-config.cmake)

        set(PCAPPLUSPLUS_LIBRARIES "${pcapplusplus_LIBRARIES}")

    elseif(USE_CMAKE_FETCH_CONTENT)
        message("Using CMAKE's FetchContent to manage source building PcapPlusPlus external lib")

        find_package(PCAP REQUIRED)
        
        # Define byproducts
        if (WIN32)
            set(BYPRODUCT1 "lib/Pcap++.lib")
            set(BYPRODUCT2 "lib/Packet++.lib")
            set(BYPRODUCT3 "lib/Common++.lib")
        elseif (APPLE)
            set(BYPRODUCT1 "lib/Plibcap++.a")
            set(BYPRODUCT2 "lib/libPacket++.a")
            set(BYPRODUCT3 "lib/libCommon++.a")
        else ()
            set(BYPRODUCT1 "lib/libPcap++.a")
            set(BYPRODUCT2 "lib/libPacket++.a")
            set(BYPRODUCT3 "lib/libCommon++.a")
        endif ()

        list(APPEND PCAPPLUSPLUS_BYPRODUCTS "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT1}" "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT2}" "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT3}")

        set(PCAPPLUSPLUS_URL https://github.com/seladb/PcapPlusPlus/archive/v22.05.tar.gz)
        set(PCAPPLUSPLUS_URL_HASH "SHA256=5f299c4503bf5d3c29f82b8d876a19be7dea29c2aadcb52f2f3b394846c21da9")

        # Set build options
        set(PCAPPLUSPLUS_BIN_DIR "${BINARY_DIR}/thirdparty/pcapplusplus-install" CACHE STRING "" FORCE)

        append_third_party_passthrough_args(PCAPPLUSPLUS_CMAKE_ARGS "${PCAPPLUSPLUS_CMAKE_ARGS}")

        if(TARGET PCAP::PCAP)
            message("Adding PCAP::PCAP as dependency to pcapplusplus-external")
        else()
            message("PCAP::PCAP target not found, so cant add as dependency to pcapplusplus-external")
        endif()

        # Create install directory before ExternalProject_Add
        file(MAKE_DIRECTORY ${PCAPPLUSPLUS_BIN_DIR})


        if (WIN32)
            # NOTE (JG): Originally, while we could run windows configure bat script, we still compiled libs for Linux or Mac
                # However, with the conan package approach for building pcapplusplus, we can add support for windows much easier
            ExternalProject_Add(
                pcapplusplus-external
                URL ${PCAPPLUSPLUS_URL}
                URL_HASH ${PCAPPLUSPLUS_URL_HASH}
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                CMAKE_ARGS ${PCAPPLUSPLUS_CMAKE_ARGS}
                CONFIGURE_COMMAND ""
                PATCH_COMMAND configure-windows-visual-studio.bat --install-dir=${BINARY_DIR}/thirdparty/pcapplusplus-install --libpcap-include-dir=${PCAP_INCLUDE_DIR} --libpcap-lib-dir=${PCAP_LIBRARY}
                BUILD_BYPRODUCTS ${PCAPPLUSPLUS_BYPRODUCTS}
                EXCLUDE_FROM_ALL TRUE
            )
        elseif (APPLE)
            ExternalProject_Add(
                pcapplusplus-external
                URL ${PCAPPLUSPLUS_URL}
                URL_HASH ${PCAPPLUSPLUS_URL_HASH}
                CMAKE_ARGS ${PCAPPLUSPLUS_CMAKE_ARGS}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/pcapplusplus-src"
                INSTALL_DIR "${BINARY_DIR}/thirdparty/pcapplusplus-install"
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS ${PCAPPLUSPLUS_BYPRODUCTS}
                CONFIGURE_COMMAND ""
                PATCH_COMMAND ./configure-mac_os_x.sh --install-dir=${BINARY_DIR}/thirdparty/pcapplusplus-install --libpcap-include-dir=${PCAP_INCLUDE_DIR} --libpcap-lib-dir=${PCAP_LIBRARY}
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
            )
        else()
            ExternalProject_Add(
                pcapplusplus-external
                URL ${PCAPPLUSPLUS_URL}
                URL_HASH ${PCAPPLUSPLUS_URL_HASH}
                CMAKE_ARGS ${PCAPPLUSPLUS_CMAKE_ARGS}
                BUILD_IN_SOURCE true
                SOURCE_DIR "${BINARY_DIR}/thirdparty/pcapplusplus-src"
                INSTALL_DIR "${BINARY_DIR}/thirdparty/pcapplusplus-install"
                LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
                BUILD_COMMAND make
                CMAKE_COMMAND ""
                UPDATE_COMMAND ""
                INSTALL_COMMAND make install
                BUILD_BYPRODUCTS ${PCAPPLUSPLUS_BYPRODUCTS}
                CONFIGURE_COMMAND ""
                PATCH_COMMAND ./configure-linux.sh --install-dir=${BINARY_DIR}/thirdparty/pcapplusplus-install --libpcap-include-dir=${PCAP_INCLUDE_DIR} --libpcap-lib-dir=${PCAP_LIBRARY}
                STEP_TARGETS build
                EXCLUDE_FROM_ALL TRUE
            )
        endif()

        add_dependencies(pcapplusplus-external libpcap-external)


        # Set variables
        set(PCAPPLUSPLUS_FOUND "YES" CACHE STRING "" FORCE)
        set(PCAPPLUSPLUS_INCLUDE_DIRS 
            "${BINARY_DIR}/thirdparty/pcapplusplus-install/include"
            "${BINARY_DIR}/thirdparty/pcapplusplus-install/include/pcapplusplus" CACHE STRING "" FORCE)
        set(PCAPPLUSPLUS_LIBRARY ${PCAPPLUSPLUS_BYPRODUCTS} CACHE STRING "" FORCE)
        set(PCAPPLUSPLUS_LIBRARIES "${PCAPPLUSPLUS_LIBRARY}" CACHE STRING "" FORCE)

        # Create imported targets
        file(MAKE_DIRECTORY ${PCAPPLUSPLUS_INCLUDE_DIRS})

        add_library(pcapplusplus::Pcap++ STATIC IMPORTED)
        set_target_properties(pcapplusplus::Pcap++ PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT1}")
        add_dependencies(pcapplusplus::Pcap++ pcapplusplus-external libpcap-external)

        add_library(pcapplusplus::Packet++ STATIC IMPORTED)
        set_target_properties(pcapplusplus::Packet++ PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT2}")
        add_dependencies(pcapplusplus::Packet++ pcapplusplus-external libpcap-external)

        add_library(pcapplusplus::Common++ STATIC IMPORTED)
        set_target_properties(pcapplusplus::Common++ PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/pcapplusplus-install/${BYPRODUCT3}")
        add_dependencies(pcapplusplus::Common++ pcapplusplus-external libpcap-external)

        add_library(pcapplusplus::pcapplusplus INTERFACE IMPORTED)
        target_link_libraries(pcapplusplus::pcapplusplus INTERFACE pcapplusplus::Pcap++ pcapplusplus::Packet++ pcapplusplus::Common++ PCAP::PCAP)
        target_include_directories(pcapplusplus::pcapplusplus INTERFACE "${PCAPPLUSPLUS_INCLUDE_DIRS}")

        if (APPLE)
            target_link_libraries(pcapplusplus::pcapplusplus INTERFACE "-framework CoreFoundation" "-framework SystemConfiguration")
            set_target_properties(pcapplusplus::pcapplusplus PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
        endif ()
    endif()

endfunction(use_bundled_pcapplusplus)