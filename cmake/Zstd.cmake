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

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)

set(PC "")
if (WIN32)
    set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/zstd/exclude_gcc_clang_compiler_options_from_windows.patch")
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${PATCH_FILE}")
endif()

FetchContent_Declare(zstd
    URL            https://github.com/facebook/zstd/archive/refs/tags/v1.5.2.tar.gz
    URL_HASH       SHA256=f7de13462f7a82c29ab865820149e778cbfe01087b3a55b5332707abf9db4a6e
    PATCH_COMMAND "${PC}"
)

# With CMake >= 3.18, this block could be replaced with FetchContent_MakeAvailable(zstd),
# if we add the `SOURCE_SUBDIR build/cmake` option to FetchContent_Declare() [this option is not available in CMake < 3.18].
# As of July 2022, one of our supported platforms, Centos 7, comes with CMake 3.17.
FetchContent_GetProperties(zstd)
if(NOT zstd_POPULATED)
    FetchContent_Populate(zstd)
    # the top level doesn't contain CMakeLists.txt, it is in the "build/cmake" subdirectory
    add_subdirectory(${zstd_SOURCE_DIR}/build/cmake ${zstd_BINARY_DIR})
endif()

add_library(zstd::zstd ALIAS libzstd_static)

# Set variables
set(ZSTD_FOUND "YES" CACHE STRING "" FORCE)
set(ZSTD_INCLUDE_DIRS "${zstd_SOURCE_DIR}/lib" CACHE STRING "" FORCE)
if (WIN32)
    set(ZSTD_LIBRARIES "${zstd_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/zstd_static.lib" CACHE STRING "" FORCE)
else()
    set(ZSTD_LIBRARIES "${zstd_BINARY_DIR}/lib/libzstd.a" CACHE STRING "" FORCE)
endif()

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZSTD_INCLUDE_DIRS=${ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZSTD_LIBRARIES=${ZSTD_LIBRARIES}" CACHE STRING "" FORCE)
