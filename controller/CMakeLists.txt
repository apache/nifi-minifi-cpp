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

cmake_minimum_required(VERSION 3.16)

include_directories(../main/ ../libminifi/include  ../libminifi/include/c2  ../libminifi/include/c2/protocols/  ../libminifi/include/core/state ./libminifi/include/core/statemanagement/metrics  ../libminifi/include/core/yaml  ../libminifi/include/core)

if(WIN32)
	include_directories(../libminifi/opsys/win)
else()
	include_directories(../libminifi/opsys/posix)
endif()

include(CppVersion)
set_cpp_version()

add_executable(minificontroller MiNiFiController.cpp ../main/MainHelper.cpp)
target_link_libraries(minificontroller ${LIBMINIFI} cxxopts Threads::Threads)

set_target_properties(minificontroller PROPERTIES
		OUTPUT_NAME minificontroller
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (NOT WIN32)
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		target_link_options(minificontroller PRIVATE "-Wl,--disable-new-dtags")
	endif()
	set_target_properties(minificontroller PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

install(TARGETS minificontroller RUNTIME DESTINATION bin COMPONENT bin)

add_custom_command(TARGET minificontroller POST_BUILD
           COMMAND cat ${CMAKE_BINARY_DIR}/all.log)
