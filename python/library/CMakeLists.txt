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

include_directories(../../nanofi/include/ ../../libminifi/include  ../../libminifi/include/c2  ../../libminifi/include/c2/protocols/  ../../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../../libminifi/include/core/yaml  ../../libminifi/include/core  ../../thirdparty/spdlog-20170710/include ../../thirdparty/concurrentqueue ../../thirdparty/yaml-cpp-yaml-cpp-20171024/include ../../thirdparty/civetweb-1.9.1/include ../../thirdparty/)
if(WIN32)
	include_directories(../../libminifi/opsys/win)
else()
	include_directories(../../libminifi/opsys/posix)
endif()
include_directories(../../extensions/http-curl/ ../../extensions/http-curl/client ../../extensions/http-curl/sitetosite ../../extensions/http-curl/protocols ../../extensions/http-curl/processors)

add_library(python-lib SHARED python_lib.cpp)
if (APPLE)
	target_link_libraries(python-lib nanofi core-minifi minifi)
else()
	target_link_libraries(python-lib nanofi-shared core-minifi-shared minifi-shared)
endif(APPLE)

if (WIN32)
target_link_libraries(python-lib ${CURL_LIBRARY}  minifi-http-curl)
    set_target_properties(python-lib PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE:minifi-http-curl"
    )
elseif (APPLE)
	target_link_libraries(python-lib ${CURL_LIBRARY}  -Wl,-all_load minifi-http-curl)
else ()
	target_link_libraries(python-lib -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive ${CURL_LIBRARY} )
endif ()