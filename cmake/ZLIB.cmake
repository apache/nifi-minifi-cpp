# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    zlib
    URL "https://github.com/madler/zlib/archive/refs/tags/v1.3.2.tar.gz"
    URL_HASH "SHA256=b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1"
    SYSTEM
)

FetchContent_MakeAvailable(zlib)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_zlib_debug_suffix "d")
else()
    set(_zlib_debug_suffix "")
endif()

if(WIN32)
    set(_zlib_static_suffix "s")
else()
    set(_zlib_static_suffix "")
endif()

set(ZLIB_FOUND "YES" CACHE STRING "" FORCE)
set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR}" CACHE STRING "" FORCE)
set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}z${_zlib_static_suffix}${_zlib_debug_suffix}${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE STRING "" FORCE)
set(ZLIB_LIBRARIES "${ZLIB_LIBRARY}" CACHE STRING "" FORCE)

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_INCLUDE_DIRS=${ZLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_ZLIB_LIBRARIES=${ZLIB_LIBRARIES}" CACHE STRING "" FORCE)
