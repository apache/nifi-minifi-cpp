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

function(use_bundled_libarchive SOURCE_DIR BINARY_DIR)
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")

    if (WIN32)
        set(BYPRODUCT "lib/archive.lib")
    else()
        set(BYPRODUCT "lib/libarchive.a")
    endif()

    set(LIBARCHIVE_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libarchive-install"
            -DCMAKE_INSTALL_LIBDIR=lib
            -DLIBARCHIVE_STATIC=1
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_MBEDTLS=OFF
            -DENABLE_NETTLE=OFF
            -DENABLE_LIBB2=OFF
            -DENABLE_LZ4=OFF
            -DENABLE_LZO=OFF
            -DENABLE_ZSTD=OFF
            -DENABLE_ZLIB=ON
            -DENABLE_LIBXML2=OFF
            -DENABLE_EXPAT=OFF
            -DENABLE_PCREPOSIX=OFF
            -DENABLE_TAR=OFF # This does not disable the tar format, just the standalone tar command line utility
            -DENABLE_CPIO=OFF
            -DENABLE_CAT=OFF
            -DENABLE_XATTR=ON
            -DENABLE_ACL=ON
            -DENABLE_ICONV=OFF
            -DENABLE_TEST=OFF
            -DENABLE_WERROR=OFF
            -DENABLE_OPENSSL=ON
            -DENABLE_UNZIP=OFF)

    if (NOT ENABLE_LZMA)
        list(APPEND LIBARCHIVE_CMAKE_ARGS -DENABLE_LZMA=OFF)
    else()
        list(APPEND LIBARCHIVE_CMAKE_ARGS -DENABLE_LZMA=ON)
    endif()

    if (NOT ENABLE_BZIP2)
        list(APPEND LIBARCHIVE_CMAKE_ARGS -DENABLE_BZip2=OFF)
    else()
        list(APPEND LIBARCHIVE_CMAKE_ARGS -DENABLE_BZip2=ON)
    endif()

    append_third_party_passthrough_args(LIBARCHIVE_CMAKE_ARGS "${LIBARCHIVE_CMAKE_ARGS}")

    ExternalProject_Add(
            libarchive-external
            URL "https://github.com/libarchive/libarchive/releases/download/v3.8.1/libarchive-3.8.1.tar.gz"
            URL_HASH "SHA256=bde832a5e3344dc723cfe9cc37f8e54bde04565bfe6f136bc1bd31ab352e9fab"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libarchive-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBARCHIVE_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    add_dependencies(libarchive-external ZLIB::ZLIB OpenSSL::Crypto)
    if (ENABLE_LZMA)
        add_dependencies(libarchive-external LibLZMA::LibLZMA)
    endif()
    if (ENABLE_BZIP2)
        add_dependencies(libarchive-external BZip2::BZip2)
    endif()

    set(LIBARCHIVE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBARCHIVE_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/libarchive-install/include" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARY "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARIES ${LIBARCHIVE_LIBRARY} CACHE STRING "" FORCE)

    add_library(LibArchive::LibArchive STATIC IMPORTED)
    set_target_properties(LibArchive::LibArchive PROPERTIES IMPORTED_LOCATION "${LIBARCHIVE_LIBRARY}")
    add_dependencies(LibArchive::LibArchive libarchive-external)
    target_link_libraries(LibArchive::LibArchive INTERFACE ZLIB::ZLIB OpenSSL::Crypto)
    if (ENABLE_LZMA)
        target_link_libraries(LibArchive::LibArchive INTERFACE LibLZMA::LibLZMA)
    endif()
    if (ENABLE_BZIP2)
        target_link_libraries(LibArchive::LibArchive INTERFACE BZip2::BZip2)
    endif()
    file(MAKE_DIRECTORY ${LIBARCHIVE_INCLUDE_DIRS})
    target_include_directories(LibArchive::LibArchive INTERFACE ${LIBARCHIVE_INCLUDE_DIRS})
    target_compile_definitions(LibArchive::LibArchive INTERFACE "LIBARCHIVE_STATIC=1")
    if (WIN32)
        target_link_libraries(LibArchive::LibArchive INTERFACE xmllite)
    endif()
endfunction(use_bundled_libarchive)
