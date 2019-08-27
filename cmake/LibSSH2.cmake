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

function(use_bundled_libssh2 SOURCE_DIR BINARY_DIR)
    message("Using bundled libssh2")

    if (WIN32)
        set(BYPRODUCT "lib/libssh2.lib")
    else()
        set(BYPRODUCT "lib/libssh2.a")
    endif()

    if (WIN32)
        string(REPLACE "/" "\\" CMAKE_CURRENT_SOURCE_DIR_BACKSLASH ${CMAKE_CURRENT_SOURCE_DIR})
        set(PC copy /Y ${CMAKE_CURRENT_SOURCE_DIR_BACKSLASH}\\thirdparty\\libssh2\\CMakeLists.txt CMakeLists.txt)
    else()
        set(PC patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2/libssh2-CMAKE_MODULE_PATH.patch)
    endif()

    set(LIBSSH2_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/libssh2-install"
            -DENABLE_ZLIB_COMPRESSION=ON
            -DCRYPTO_BACKEND=OpenSSL
            -DBUILD_TESTING=OFF
            -DBUILD_EXAMPLES=OFF)

    list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl)
    list(APPEND LIBSSH2_CMAKE_ARGS "-DLIBRESSL_BIN_DIR=${LIBRESSL_BIN_DIR}"
            "-DLIBRESSL_SRC_DIR=${LIBRESSL_SRC_DIR}"
            "-DBYPRODUCT_PREFIX=${BYPRODUCT_PREFIX}"
            "-DBYPRODUCT_SUFFIX=${BYPRODUCT_SUFFIX}")
    if(NOT USE_SYSTEM_ZLIB OR USE_SYSTEM_ZLIB STREQUAL "OFF")
        list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zlib/dummy)
        list(APPEND LIBSSH2_CMAKE_ARGS "-DZLIB_BYPRODUCT_INCLUDE=${ZLIB_BYPRODUCT_INCLUDE}"
                "-DZLIB_BYPRODUCT=${ZLIB_BYPRODUCT}")
    endif()
    if(CMAKE_MODULE_PATH_PASSTHROUGH_LIST)
        string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH_PASSTHROUGH_LIST}")
        list(APPEND LIBSSH2_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    endif()

    ExternalProject_Add(
            libssh2-external
            URL "https://www.libssh2.org/download/libssh2-1.8.2.tar.gz"
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/libssh2-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBSSH2_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/libssh2-install/${BYPRODUCT}"
    )

    add_dependencies(libssh2-external libressl-portable)
    if(NOT USE_SYSTEM_ZLIB OR USE_SYSTEM_ZLIB STREQUAL "OFF")
        add_dependencies(libssh2-external z)
    endif()

    set(LIBSSH2_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2/" CACHE STRING "" FORCE)
    set(LIBSSH2_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/libssh2-install/" CACHE STRING "" FORCE)
    set(LIBSSH2_BYPRODUCT_DIR "${BYPRODUCT}" CACHE STRING "" FORCE)

    add_library(libssh2 STATIC IMPORTED)
    set_target_properties(libssh2 PROPERTIES IMPORTED_LOCATION "${LIBSSH2_BIN_DIR}${BYPRODUCT}")

    if (OPENSSL_FOUND)
        if (NOT WIN32)
            set_target_properties(libssh2 PROPERTIES INTERFACE_LINK_LIBRARIES "${OPENSSL_LIBRARIES}")
        endif()
    endif(OPENSSL_FOUND)
    if (ZLIB_FOUND)
        if (NOT WIN32)
            set_target_properties(libssh2 PROPERTIES INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARIES}")
        endif()
    endif(ZLIB_FOUND)
    add_dependencies(libssh2 libssh2-external)
    set(LIBSSH2_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBSSH2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libssh2/include" CACHE STRING "" FORCE)
    set(LIBSSH2_LIBRARY "${LIBSSH2_BIN_DIR}${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBSSH2_LIBRARIES ${LIBSSH2_LIBRARY} CACHE STRING "" FORCE)
endfunction(use_libre_ssl)