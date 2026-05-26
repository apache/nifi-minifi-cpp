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

include(FetchContent)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/zstd/dummy")

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(zstd
    URL            https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz
    URL_HASH       SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3
    SOURCE_SUBDIR  build/cmake
    SYSTEM
)

FetchContent_MakeAvailable(zstd)

if (NOT TARGET zstd::zstd)
    add_library(zstd::zstd ALIAS libzstd_static)
endif()

# Set variables
set(ZSTD_FOUND "YES" CACHE STRING "" FORCE)
set(ZSTD_INCLUDE_DIRS "${zstd_SOURCE_DIR}/lib" CACHE STRING "" FORCE)
if (WIN32)
    if (CMAKE_GENERATOR STREQUAL "Ninja")
        set(ZSTD_LIBRARIES "${zstd_BINARY_DIR}/lib/zstd_static.lib" CACHE STRING "" FORCE)
        set(ZSTD_LIBRARY "${zstd_BINARY_DIR}/lib/zstd_static.lib" CACHE STRING "" FORCE)
    else()
        set(ZSTD_LIBRARIES "${zstd_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/zstd_static.lib" CACHE STRING "" FORCE)
        set(ZSTD_LIBRARY "${zstd_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/zstd_static.lib" CACHE STRING "" FORCE)
    endif()
else()
    set(ZSTD_LIBRARIES "${zstd_BINARY_DIR}/lib/libzstd.a" CACHE STRING "" FORCE)
    set(ZSTD_LIBRARY "${zstd_BINARY_DIR}/lib/libzstd.a" CACHE STRING "" FORCE)
endif()

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZSTD_INCLUDE_DIRS=${ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZSTD_LIBRARIES=${ZSTD_LIBRARIES}" CACHE STRING "" FORCE)
