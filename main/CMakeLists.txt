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

IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

include_directories(../libminifi/include  ../libminifi/include/core/yaml  ../libminifi/include/core ../thirdparty/spdlog-0.13.0/include ../thirdparty/concurrentqueue ../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ../thirdparty/civetweb-1.9.1/include ../thirdparty/jsoncpp/include ../thirdparty/leveldb-1.18/include ../thirdparty/)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(minifiexe MiNiFiMain.cpp)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifiexe "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifiexe "${CMAKE_THREAD_LIBS_INIT}")
endif()

# Include UUID
find_package(UUID REQUIRED)

# Include OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl, jsoncpp and leveldb
target_link_libraries(minifiexe minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB} ${UUID_LIBRARIES} ${LEVELDB_LIBRARIES} ${OPENSSL_LIBRARIES})
set_target_properties(minifiexe
        PROPERTIES OUTPUT_NAME minifi)

install(TARGETS minifiexe
        RUNTIME
        DESTINATION bin
        COMPONENT bin)
