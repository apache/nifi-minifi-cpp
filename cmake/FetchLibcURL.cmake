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

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/curl/module-path.patch")
set(PC "${Patch_EXECUTABLE}" -p1 -i "${PATCH_FILE}")

FetchContent_Declare(
        curl
        URL "https://github.com/curl/curl/releases/download/curl-8_19_0/curl-8.19.0.tar.gz"
        URL_HASH "SHA256=2a2c11db4c122691aa23b4363befda1bfd801770bfebf41e1d21cee4f2ab0f71"
        PATCH_COMMAND ${PC}
        SYSTEM
        OVERRIDE_FIND_PACKAGE
)

set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CURL_CA_PATH "none" CACHE STRING "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(curl)

if (TARGET libcurl_static)
    target_compile_definitions(libcurl_static PUBLIC CURL_STATICLIB)

    if (APPLE)
        target_link_libraries(libcurl_static INTERFACE "-framework CoreFoundation -framework SystemConfiguration -framework CoreServices")
    elseif (WIN32)
        target_link_libraries(libcurl_static INTERFACE Iphlpapi.lib)
    endif ()
else()
    message(WARNING "libcurl_static target not found; bundled curl may not link correctly.")
endif()

# --- EXPORT LEGACY VARIABLES ---
set(CURL_FOUND "YES" CACHE INTERNAL "")
set(CURL_INCLUDE_DIR "${curl_SOURCE_DIR}/include" CACHE INTERNAL "")
set(CURL_INCLUDE_DIRS "${curl_SOURCE_DIR}/include" CACHE INTERNAL "")
set(CURL_LIBRARY CURL::libcurl CACHE INTERNAL "")
set(CURL_LIBRARIES CURL::libcurl CACHE INTERNAL "")
