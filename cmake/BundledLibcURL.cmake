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
    set(PATCH_FILE_1 "${SOURCE_DIR}/thirdparty/curl/module-path.patch")
    set(PC ${Bash_EXECUTABLE} -c "set -x && \
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE_1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE_1}\")")
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/libcurl.lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
        set(BYPRODUCT "${LIBDIR}/libcurl.a")
    endif()

    # Set build options
    set(CURL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/curl-install"
            -DBUILD_CURL_EXE=OFF
            -DBUILD_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DHTTP_ONLY=ON
            -DCURL_CA_PATH=none
            -DCURL_USE_LIBSSH2=OFF
            -DUSE_LIBIDN2=OFF
            -DCURL_USE_LIBPSL=OFF
            )
    if (OPENSSL_OFF)
        list(APPEND CURL_CMAKE_ARGS -DCURL_USE_OPENSSL=OFF)
    else()
        list(APPEND CURL_CMAKE_ARGS -DCURL_USE_OPENSSL=ON)
    endif()

    append_third_party_passthrough_args(CURL_CMAKE_ARGS "${CURL_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            curl-external
            URL "https://github.com/curl/curl/releases/download/curl-8_4_0/curl-8.4.0.tar.gz"
            URL_HASH "SHA256=816e41809c043ff285e8c0f06a75a1fa250211bbfb2dc0a037eeef39f1a9e427"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/curl-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CURL_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
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
    target_include_directories(CURL::libcurl INTERFACE ${CURL_INCLUDE_DIRS})
    target_link_libraries(CURL::libcurl INTERFACE ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        target_link_libraries(CURL::libcurl INTERFACE "-framework CoreFoundation")
        target_link_libraries(CURL::libcurl INTERFACE "-framework SystemConfiguration")
    endif()
    if (NOT OPENSSL_OFF)
        target_link_libraries(CURL::libcurl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endfunction(use_bundled_curl SOURCE_DIR BINARY_DIR)
