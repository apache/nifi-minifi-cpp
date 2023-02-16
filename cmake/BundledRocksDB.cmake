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

function(use_bundled_rocksdb SOURCE_DIR BINARY_DIR)
    message("Using bundled RocksDB")

    if (NOT WIN32)
        include(Zstd)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/zstd/dummy")

        include(LZ4)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/lz4/dummy")
    endif()

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/rocksdb.lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
        set(BYPRODUCT "${LIBDIR}/librocksdb.a")
    endif()

    # Set build options
    set(ROCKSDB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/rocksdb-install"
            -DWITH_TESTS=OFF
            -DWITH_TOOLS=ON
            -DWITH_GFLAGS=OFF
            -DUSE_RTTI=1
            -DROCKSDB_BUILD_SHARED=OFF
            -DFAIL_ON_WARNINGS=OFF
            -DCMAKE_CXX_STANDARD=17  # RocksDB fails to build in C++20 mode on GCC 11: https://godbolt.org/z/YeMcEzs8W
            )
    if(PORTABLE)
        list(APPEND ROCKSDB_CMAKE_ARGS -DPORTABLE=ON)
    endif()
    if(WIN32)
        list(APPEND ROCKSDB_CMAKE_ARGS
                -DROCKSDB_INSTALL_ON_WINDOWS=ON
                -DWITH_XPRESS=ON)
    else()
        list(APPEND ROCKSDB_CMAKE_ARGS
                -DWITH_ZLIB=ON
                -DWITH_BZ2=ON
                -DWITH_ZSTD=ON
                -DWITH_LZ4=ON)
    endif()

    append_third_party_passthrough_args(ROCKSDB_CMAKE_ARGS "${ROCKSDB_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            rocksdb-external
            URL "https://github.com/facebook/rocksdb/archive/refs/tags/v7.7.3.tar.gz"
            URL_HASH "SHA256=b8ac9784a342b2e314c821f6d701148912215666ac5e9bdbccd93cf3767cb611"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/rocksdb-src"
            CMAKE_ARGS ${ROCKSDB_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/rocksdb-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
    )

    # Set variables
    set(ROCKSDB_FOUND "YES" CACHE STRING "" FORCE)
    set(ROCKSDB_INCLUDE_DIR "${BINARY_DIR}/thirdparty/rocksdb-install/include" CACHE STRING "" FORCE)
    set(ROCKSDB_LIBRARY "${BINARY_DIR}/thirdparty/rocksdb-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(ROCKSDB_LIBRARIES ${ROCKSDB_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(RocksDB::RocksDB STATIC IMPORTED)
    set_target_properties(RocksDB::RocksDB PROPERTIES IMPORTED_LOCATION "${ROCKSDB_LIBRARY}")
    if (NOT WIN32)
        add_dependencies(rocksdb-external ZLIB::ZLIB BZip2::BZip2 zstd::zstd lz4::lz4)
    endif()
    add_dependencies(RocksDB::RocksDB rocksdb-external)
    file(MAKE_DIRECTORY ${ROCKSDB_INCLUDE_DIR})
    target_include_directories(RocksDB::RocksDB INTERFACE ${ROCKSDB_INCLUDE_DIR})
    set_property(TARGET RocksDB::RocksDB APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
    target_link_libraries(RocksDB::RocksDB INTERFACE Threads::Threads)
    if(WIN32)
        target_link_libraries(RocksDB::RocksDB INTERFACE Rpcrt4.lib Cabinet.lib)
    else()
        target_link_libraries(RocksDB::RocksDB INTERFACE ZLIB::ZLIB BZip2::BZip2 zstd::zstd lz4::lz4)
    endif()
endfunction(use_bundled_rocksdb)
