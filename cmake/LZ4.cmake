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

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/lz4/dummy")

set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(lz4
    URL            https://github.com/lz4/lz4/archive/refs/tags/v1.9.4.tar.gz
    URL_HASH       SHA256=0b0e3aa07c8c063ddf40b082bdf7e37a1562bda40a0ff5272957f3e987e0e54b
    SOURCE_SUBDIR  build/cmake
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(lz4)

if(NOT TARGET lz4::lz4)
    add_library(lz4::lz4 ALIAS lz4_static)
    add_library(LZ4::LZ4 ALIAS lz4_static)

    install(TARGETS lz4_static
            EXPORT Lz4Targets          # Add to Lz4Targets export set
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
    )

    # Install the export set itself
    install(EXPORT Lz4Targets
            FILE Lz4Targets.cmake       # Generate Lz4Targets.cmake file
            NAMESPACE lz4::             # Set the namespace to lz4::
            DESTINATION lib/cmake/Lz4   # Where to install the CMake config
    )
endif()

# Set variables
set(LZ4_FOUND "YES" CACHE STRING "" FORCE)
set(LZ4_INCLUDE_DIRS "${lz4_SOURCE_DIR}/lib" CACHE STRING "" FORCE)
if (WIN32)
    set(LZ4_LIBRARIES "${lz4_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/lz4_static.lib" CACHE STRING "" FORCE)
else()
    set(LZ4_LIBRARIES "${lz4_BINARY_DIR}/liblz4.a" CACHE STRING "" FORCE)
endif()

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LZ4_INCLUDE_DIRS=${LZ4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LZ4_LIBRARIES=${LZ4_LIBRARIES}" CACHE STRING "" FORCE)
