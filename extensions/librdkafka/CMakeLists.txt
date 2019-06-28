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

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-rdkafka-extensions STATIC ${SOURCES})
set_property(TARGET minifi-rdkafka-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-rdkafka-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-rdkafka-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

  set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/kafka")
  if (WIN32)
  	set(BYPRODUCT "${BASE_DIR}/install/lib/librdkafka.lib")
  else()
  	set(BYPRODUCT "${BASE_DIR}/install/lib/librdkafka.a")
  endif()

list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST "${CMAKE_SOURCE_DIR}/cmake/ssl")
list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST "${CMAKE_SOURCE_DIR}/cmake/zlib/dummy")
    
ExternalProject_Add(
    kafka-external
    GIT_REPOSITORY "https://github.com/edenhill/librdkafka.git"
    GIT_TAG "v1.0.1" 
    PREFIX "${BASE_DIR}"
    CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
               "-DCMAKE_INSTALL_PREFIX=${BASE_DIR}/install"
               "-DWITH_SASL=OFF"
               "-DOPENSSL_VERSION=1.0.2"
               "-DRDKAFKA_BUILD_STATIC=ON"
               "-DRDKAFKA_BUILD_EXAMPLES=OFF"
               "-DRDKAFKA_BUILD_TESTS=OFF"
               "-DENABLE_LZ4_EXT=OFF"
               "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH_LIST}"
               "-DCMAKE_C_FLAGS=${CURL_C_FLAGS}"
               "-DCMAKE_INSTALL_LIBDIR=lib"
               "-DCMAKE_CXX_FLAGS=${CURL_CXX_FLAGS}"
    EXCLUDE_FROM_ALL TRUE
  )
if ( NOT USE_SYSTEM_OPENSSL )
	add_dependencies(kafka-external libressl-portable)
endif()
set(KAFKA_INCLUDE "${BASE_DIR}/install/include/librdkafka/")
add_dependencies(minifi-rdkafka-extensions kafka-external)
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${KAFKA_INCLUDE})
target_link_libraries (minifi-rdkafka-extensions ${BYPRODUCT})

SET (RDKAFKA-EXTENSIONS minifi-rdkafka-extensions PARENT_SCOPE)

register_extension(minifi-rdkafka-extensions)

register_extension_linter(minifi-rdkafka-extensions-linter)
