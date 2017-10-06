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

include_directories(../../libminifi/include ../../libminifi/include/c2  ../../libminifi/include/c2/protocols/  ../../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../../libminifi/include/core/yaml  ../../libminifi/include/core  ../../thirdparty/spdlog-20170710/include ../../thirdparty/concurrentqueue ../../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ../../thirdparty/civetweb-1.9.1/include ../../thirdparty/jsoncpp/include  ../../thirdparty/)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB SOURCES  "*.cpp" "protocols/*.cpp" "client/*.cpp" "processors/*.cpp")

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
        target_link_libraries (minifi-http-curl ${CURL_LIBRARIES})
endif(CURL_FOUND)

# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-http-curl ${LIBMINIFI} ${UUID_LIBRARIES} ${JSONCPP_LIB})
add_dependencies(minifi-http-curl jsoncpp_project)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(minifi-http-curl ${CMAKE_DL_LIBS} )
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


