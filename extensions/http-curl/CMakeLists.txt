#
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
#

if(NOT USE_SYSTEM_CURL)
  message("Using bundled cURL")

  set(CURL_C_FLAGS "-I${OPENSSL_INCLUDE_DIR}")
  set(CURL_CXX_FLAGS "${CURL_C_FLAGS}")

  ExternalProject_Add(
    curl-external
    GIT_REPOSITORY "https://github.com/curl/curl.git"
    GIT_TAG "4d6bd91ab33328c6d27eddc32e064defc02dc4fd"  # Version 7.59.0
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-src"
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install"
                -DBUILD_CURL_EXE=OFF
                -DBUILD_TESTING=OFF
                -DCURL_STATICLIB=ON
                -DHTTP_ONLY=ON
                -DCURL_DISABLE_CRYPTO_AUTH=ON
                -DHAVE_GLIBC_STRERROR_R=1
                -DHAVE_GLIBC_STRERROR_R__TRYRUN_OUTPUT=""
                -DHAVE_POSIX_STRERROR_R=0
                -DHAVE_POSIX_STRERROR_R__TRYRUN_OUTPUT=""
                -DHAVE_POLL_FINE_EXITCODE=0
                -DHAVE_FSETXATTR_5=0
                -DHAVE_FSETXATTR_5__TRYRUN_OUTPUT=""
               "-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ssl"
               "-DCMAKE_C_FLAGS=${CURL_C_FLAGS}"
               "-DCMAKE_CXX_FLAGS=${CURL_CXX_FLAGS}"
    BUILD_BYPRODUCTS "thirdparty/curl-install/lib/libcurl.a"
  )

  if(NOT USE_SYSTEM_OPENSSL)
    add_dependencies(curl-external libressl-portable)
  endif()

  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/curl/dummy")
  add_library(curl STATIC IMPORTED)
  set_target_properties(curl PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install/lib/libcurl.a")
  set_target_properties(curl PROPERTIES INTERFACE_LINK_LIBRARIES ${OPENSSL_LIBRARIES})
  add_dependencies(curl curl-external)
  set(CURL_FOUND "YES")
  set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/curl/include")
  set(CURL_LIBRARIES curl)
endif()

find_package(CURL REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-all-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--export-symbols")

include_directories(../../libminifi/include  ../../libminifi/include/core/yaml  ../../libminifi/include/core  ../../thirdparty/spdlog-20170710/include ../../thirdparty/concurrentqueue ../../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ${CIVET_THIRDPARTY_ROOT}/include ../../thirdparty/)
include_directories(protocols client processors sitetosite)

file(GLOB SOURCES  "*.cpp" "protocols/*.cpp" "client/*.cpp" "processors/*.cpp" "sitetosite/*.cpp")

if (USE_CURL_NSS)
    message("okay use nss")
	add_definitions(-DUSE_CURL_NSS)
endif()

add_library(minifi-http-curl STATIC ${SOURCES})
set_property(TARGET minifi-http-curl PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-http-curl "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-http-curl "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (CURL_FOUND)
  include_directories(${CURL_INCLUDE_DIRS})
  target_link_libraries(minifi-http-curl ${CURL_LIBRARIES})
endif ()

# Include UUID
find_package(UUID REQUIRED)
#set(LINK_FLAGS ${LINK_FLAGS} "-Wl,-whole-archive")

# Include OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(minifi-http-curl ${CMAKE_DL_LIBS} ${OPENSSL_LIBRARIES})
target_link_libraries(minifi-http-curl minifi-civet-extensions)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-http-curl ${ZLIB_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-http-curl PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-http-curl PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-http-curl PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

SET (HTTP-CURL minifi-http-curl PARENT_SCOPE)
register_extension(minifi-http-curl)
