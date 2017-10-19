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

include_directories(../libminifi/include  ../libminifi/include/c2  ../libminifi/include/c2/protocols/  ../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../libminifi/include/core/yaml  ../libminifi/include/core  ../thirdparty/spdlog-20170710/include ../thirdparty/concurrentqueue ../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ../thirdparty/civetweb-1.9.1/include ../thirdparty/jsoncpp/include ../thirdparty/libarchive-3.3.2/libarchive ../thirdparty/)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Os")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Os")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

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

# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl, jsoncpp and rocksdb
target_link_libraries(minifiexe minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB} ${UUID_LIBRARIES} ${OPENSSL_LIBRARIES} archive)

if (APPLE)
	if (HTTP-CURL)    
    	target_link_libraries (minifiexe -Wl,-all_load ${HTTP-CURL})
    endif()
    if (ROCKSDB-REPOS)    
    	target_link_libraries (minifiexe -Wl,-all_load ${ROCKSDB-REPOS})
    endif()
    
else ()
	if (HTTP-CURL)
	  target_link_libraries (minifiexe -Wl,--whole-archive ${HTTP-CURL} -Wl,--no-whole-archive)
	endif()
	if (ROCKSDB-REPOS)
	  target_link_libraries (minifiexe -Wl,--whole-archive ${ROCKSDB-REPOS} -Wl,--no-whole-archive)
	endif()        
endif ()

set_property(TARGET minifiexe PROPERTY INTERPROCEDURAL_OPTIMIZATION True)

set_target_properties(minifiexe
        PROPERTIES OUTPUT_NAME minifi)



install(TARGETS minifiexe
        RUNTIME
        DESTINATION bin
        COMPONENT bin)


add_custom_command(TARGET minifiexe POST_BUILD
           COMMAND cat ${CMAKE_BINARY_DIR}/all.log)
