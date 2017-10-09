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

cmake_minimum_required(VERSION 2.6)

find_package(CURL REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-all-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--export-symbols")

include_directories(../../libminifi/include ../../libminifi/include/c2  ../../libminifi/include/c2/protocols/  ../../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../../libminifi/include/core/yaml  ../../libminifi/include/core  ../../thirdparty/spdlog-20170710/include ../../thirdparty/concurrentqueue ../../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ../../thirdparty/civetweb-1.9.1/include ../../thirdparty/jsoncpp/include ../../thirdparty/rocksdb/include  ../../thirdparty/libarchive-3.3.2/libarchive ../../thirdparty/)

find_package(Boost REQUIRED)
find_package(RocksDB)

SET_PACKAGE_INFO("HTTP-CURL" "libcURL implementation code that supports " )

include_directories(${Boost_INCLUDE_DIRS})

file(GLOB SOURCES  "*.cpp")

add_library(minifi-rocksdb-repos STATIC ${SOURCES})
set_property(TARGET minifi-rocksdb-repos PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-rocksdb-repos "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-rocksdb-repos "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIRS})
        target_link_libraries (minifi-rocksdb-repos ${CURL_LIBRARIES})
endif(CURL_FOUND)

# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-rocksdb-repos ${LIBMINIFI} ${UUID_LIBRARIES} ${JSONCPP_LIB})
add_dependencies(minifi-rocksdb-repos jsoncpp_project)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(minifi-rocksdb-repos ${CMAKE_DL_LIBS} )
if (ROCKSDB_FOUND)
	target_link_libraries(minifi-rocksdb-repos ${ROCKSDB_LIBRARIES} )
else()
	target_link_libraries(minifi-rocksdb-repos rocksdb )
endif()
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-rocksdb-repos ${ZLIB_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (ROCKSDB-REPOS minifi-rocksdb-repos PARENT_SCOPE)


