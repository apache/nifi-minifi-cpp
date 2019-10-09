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

function(use_bundled_curl SOURCE_DIR BINARY_DIR)
    # Define patch step
    if (WIN32)
        set(PC "PATCH_COMMAND ./buildconf.bat")
    endif()

    # Define byproducts
    get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

    if ("${LIB64}" STREQUAL "TRUE" AND (NOT WIN32 AND NOT APPLE))
        set(LIBSUFFIX 64)
    endif()

    if (WIN32)
        set(BYPRODUCT "lib/libcurl.lib")
    else()
        set(BYPRODUCT "lib${LIBSUFFIX}/libcurl.a")
    endif()

    # Set build options
    set(CURL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/curl-install"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_CURL_EXE=OFF
            -DBUILD_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DHTTP_ONLY=ON
            -DCURL_DISABLE_CRYPTO_AUTH=ON
            -DCURL_CA_PATH=none
            -DCMAKE_USE_LIBSSH2=OFF
            -DCMAKE_DEBUG_POSTFIX=
            -DHAVE_GLIBC_STRERROR_R=1
            -DHAVE_GLIBC_STRERROR_R__TRYRUN_OUTPUT=""
            -DHAVE_POSIX_STRERROR_R=0
            -DHAVE_POSIX_STRERROR_R__TRYRUN_OUTPUT=""
            -DHAVE_POLL_FINE_EXITCODE=0
            -DHAVE_FSETXATTR_5=0
            -DHAVE_FSETXATTR_5__TRYRUN_OUTPUT=""
            )
    if (OPENSSL_OFF)
        list(APPEND CURL_CMAKE_ARGS -DCMAKE_USE_OPENSSL=OFF)
    else()
        list(APPEND CURL_CMAKE_ARGS -DCMAKE_USE_OPENSSL=ON)
    endif()

    append_third_party_passthrough_args(CURL_CMAKE_ARGS "${CURL_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            curl-external
            URL "https://curl.haxx.se/download/curl-7.64.0.tar.gz"
            URL_HASH "SHA256=cb90d2eb74d4e358c1ed1489f8e3af96b50ea4374ad71f143fa4595e998d81b5"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/curl-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CURL_CMAKE_ARGS}
            ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/curl-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(curl-external ZLIB::ZLIB)
    if (NOT OPENSSL_OFF)
        add_dependencies(curl-external OpenSSL::SSL OpenSSL::Crypto)
    endif()

    # Set variables
    set(CURL_FOUND "YES" CACHE STRING "" FORCE)
    set(CURL_INCLUDE_DIR "${BINARY_DIR}/thirdparty/curl-install/include" CACHE STRING "" FORCE)
    set(CURL_INCLUDE_DIRS "${CURL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(CURL_LIBRARY "${BINARY_DIR}/thirdparty/curl-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(CURL_LIBRARIES "${CURL_LIBRARY}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_CURL_INCLUDE_DIR=${CURL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_CURL_LIBRARY=${CURL_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${CURL_INCLUDE_DIRS})

    add_library(CURL::libcurl STATIC IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_LOCATION "${CURL_LIBRARY}")
    add_dependencies(CURL::libcurl curl-external)
    set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CURL_INCLUDE_DIRS})
    set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
    if (NOT OPENSSL_OFF)
        set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    endif()
endfunction(use_bundled_curl SOURCE_DIR BINARY_DIR)
