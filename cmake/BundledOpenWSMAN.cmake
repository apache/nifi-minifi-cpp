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

function(use_bundled_openwsman SOURCE_DIR BINARY_DIR)
    message("Using bundled openwsman")

    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/openwsman/openwsman.patch")

    # Define byproducts
    if (APPLE)
        set(PREFIX "lib/lib")
        set(POSTFIX ".a")
    elseif(WIN32)
        message(FATAL_ERROR "OpenWSMAN Windows build is not supported")
    else()
        if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            set(PREFIX "lib64/lib")
        else()
            set(PREFIX "lib/lib")
        endif()
        set(POSTFIX ".a")
    endif()

    set(BYPRODUCTS
            "${PREFIX}wsman${POSTFIX}"
            "${PREFIX}wsman_client${POSTFIX}"
            "${PREFIX}wsman_curl_client_transport${POSTFIX}"
            )

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND OPENWSMAN_LIBRARIES_LIST "${BINARY_DIR}/thirdparty/openwsman-install/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(OPENWSMAN_CMAKE_ARGS
            ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/openwsman-install"
            -DBUILD_PYTHON=NO
            -DBUILD_PYTHON3=NO
            -DBUILD_LIBCIM=NO
            -DBUILD_EXAMPLES=NO
            -DBUILD_BINDINGS=NO
            -DBUILD_RUBY=NO
            -DBUILD_PERL=NO
            -DBUILD_JAVA=NO
            -DBUILD_CSHARP=NO
            -DBUILD_CUNIT_TESTS=NO
            -DDISABLE_PLUGINS=YES
            -DUSE_PAM=NO
            -DBUILD_TESTS=NO
            -DDISABLE_SERVER=YES
            -DBUILD_SHARED_LIBS=NO)

    append_third_party_passthrough_args(OPENWSMAN_CMAKE_ARGS "${OPENWSMAN_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            openwsman-external
            URL "https://github.com/Openwsman/openwsman/archive/v2.6.11.tar.gz"
            URL_HASH "SHA256=895eaaae62925f9416766ea3e71a5368210e6cfe13b23e4e0422fa0e75c2541c"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/openwsman-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${OPENWSMAN_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${OPENWSMAN_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(openwsman-external LibXml2::LibXml2 OpenSSL::SSL OpenSSL::Crypto CURL::libcurl)

    # Set variables
    set(OPENWSMAN_FOUND "YES" CACHE STRING "" FORCE)
    set(OPENWSMAN_INCLUDE_DIR "${BINARY_DIR}/thirdparty/openwsman-src/include" CACHE STRING "" FORCE)
    set(OPENWSMAN_LIBRARIES "${OPENWSMAN_LIBRARIES_LIST}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${OPENWSMAN_INCLUDE_DIR})

    add_library(OpenWSMAN::libwsman_curl_client_transport STATIC IMPORTED)
    set_target_properties(OpenWSMAN::libwsman_curl_client_transport PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/openwsman-install/${PREFIX}wsman_curl_client_transport${POSTFIX}")
    add_dependencies(OpenWSMAN::libwsman_curl_client_transport openwsman-external)
    set_property(TARGET OpenWSMAN::libwsman_curl_client_transport APPEND PROPERTY INTERFACE_LINK_LIBRARIES LibXml2::LibXml2 OpenSSL::SSL OpenSSL::Crypto CURL::libcurl)
    set_property(TARGET OpenWSMAN::libwsman_curl_client_transport APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENWSMAN_INCLUDE_DIR})

    add_library(OpenWSMAN::libwsman_client STATIC IMPORTED)
    set_target_properties(OpenWSMAN::libwsman_client PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/openwsman-install/${PREFIX}wsman_client${POSTFIX}")
    add_dependencies(OpenWSMAN::libwsman_client openwsman-external)
    set_property(TARGET OpenWSMAN::libwsman_client APPEND PROPERTY INTERFACE_LINK_LIBRARIES LibXml2::LibXml2)
    set_property(TARGET OpenWSMAN::libwsman_client APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenWSMAN::libwsman_curl_client_transport)
    set_property(TARGET OpenWSMAN::libwsman_client APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENWSMAN_INCLUDE_DIR})

    add_library(OpenWSMAN::libwsman STATIC IMPORTED)
    set_target_properties(OpenWSMAN::libwsman PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/openwsman-install/${PREFIX}wsman${POSTFIX}")
    add_dependencies(OpenWSMAN::libwsman openwsman-external)
    set_property(TARGET OpenWSMAN::libwsman APPEND PROPERTY INTERFACE_LINK_LIBRARIES LibXml2::LibXml2)
    set_property(TARGET OpenWSMAN::libwsman APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenWSMAN::libwsman_client)
    set_property(TARGET OpenWSMAN::libwsman APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENWSMAN_INCLUDE_DIR})
endfunction(use_bundled_openwsman)
