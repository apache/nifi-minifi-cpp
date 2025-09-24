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

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_DEBUG OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_CATALOG OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_ICU OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    libxml2
    URL https://github.com/GNOME/libxml2/archive/refs/tags/v2.15.0.tar.gz
    URL_HASH SHA256=e24bd5209afefe390e704ebc55649c7ae240e1f157cefd433ccc86c610d20aac
    SYSTEM
)
FetchContent_MakeAvailable(libxml2)

set(LIBXML2_FOUND "YES" CACHE STRING "" FORCE)
set(LIBXML2_INCLUDE_DIRS
    "${libxml2_SOURCE_DIR}/include"
    "${libxml2_BINARY_DIR}"
    CACHE STRING "" FORCE)
if (WIN32)
    set(LIBXML2_LIBRARIES "${libxml2_BINARY_DIR}/libxml2s.lib" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARY "${libxml2_BINARY_DIR}/libxml2s.lib" CACHE STRING "" FORCE)
else()
    set(LIBXML2_LIBRARIES "${libxml2_BINARY_DIR}/libxml2.a" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARY "${libxml2_BINARY_DIR}/libxml2.a" CACHE STRING "" FORCE)
endif()

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBXML2_INCLUDE_DIRS=${LIBXML2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBXML2_LIBRARIES=${LIBXML2_LIBRARIES}" CACHE STRING "" FORCE)
