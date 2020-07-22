# Licensed to the Apache Software Foundation (ASF) under one
#
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

cmake_minimum_required (VERSION 2.6)

project(nifi-libcore-minifi)
set(PROJECT_NAME "nifi-libcore-minifi")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 6)
set(PROJECT_VERSION_PATCH 0)


#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()
IF (IOS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-abi-version=2 -fobjc-arc -std=gnu++11 -stdlib=libc++ -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-abi-version=2 -fobjc-arc -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
ELSE ()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DYAML_SUPPORT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DYAML_SUPPORT")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBDIFF")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBDIFF")
#include_directories("../thirdparty/bsdiff/")

ENDIF()


include(CheckCXXCompilerFlag)
if (WIN32)
  if ((MSVC_VERSION GREATER "1900") OR (MSVC_VERSION EQUAL "1900"))
	    CHECK_CXX_COMPILER_FLAG("/std:c++14" _cpp_latest_flag_supported)
	    if (_cpp_latest_flag_supported)
	        add_compile_options("/std:c++14")
	    endif()
	endif()
else()
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

endif()

if (WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-reorder")
endif()

include_directories(include)


if(WIN32)
include_directories(opsys/win)
set(SOCKET_SOURCES "src/io/win/*.cpp")
else()
include_directories(opsys/posix)
set(SOCKET_SOURCES "src/io/posix/*.cpp")
endif()

if (NOT OPENSSL_OFF)
	set(TLS_SOURCES "src/io/tls/*.cpp")
endif()

file(GLOB SOURCES  "src/utils/file/*.cpp" "src/sitetosite/*.cpp"  "src/core/logging/*.cpp"  "src/core/state/*.cpp" "src/core/state/nodes/*.cpp" "src/c2/protocols/*.cpp" "src/c2/triggers/*.cpp" "src/c2/*.cpp" "src/io/*.cpp" ${SOCKET_SOURCES} ${TLS_SOURCES} "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/controllers/keyvalue/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp"  "src/provenance/*.cpp" "src/utils/*.cpp" "src/agent/agent_version.cpp" "src/*.cpp")

if(WIN32)
	include(FindMessageCompiler)
	find_package(MessageCompiler REQUIRED)
	add_custom_target(message-strings
						COMMAND "${CMAKE_MC_COMPILER}" -U "${CMAKE_CURRENT_SOURCE_DIR}/src/core/logging/WindowsMessageTextFile.mc" -h "${CMAKE_CURRENT_BINARY_DIR}/include/core/logging" -r "${CMAKE_CURRENT_BINARY_DIR}/src/core/logging"
						DEPENDS "src/core/logging/WindowsMessageTextFile.mc"
						BYPRODUCTS "include/core/logging/WindowsMessageTextFile.h" "src/core/logging/WindowsMessageTextFile.rc")
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/core/logging")
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/core/logging")
endif()

file(GLOB PROCESSOR_SOURCES  "src/processors/*.cpp" )


add_library(core-minifi STATIC ${SOURCES})
if(WIN32)
        add_dependencies(core-minifi message-strings)
        target_include_directories(core-minifi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
endif()

list(APPEND LIBMINIFI_LIBRARIES yaml-cpp ZLIB::ZLIB concurrentqueue RapidJSON spdlog cron Threads::Threads gsl-lite optional-lite)
if(NOT WIN32)
	list(APPEND LIBMINIFI_LIBRARIES OSSP::libuuid++)
endif()
if (NOT OPENSSL_OFF)
	list(APPEND LIBMINIFI_LIBRARIES OpenSSL::SSL)
endif()
target_link_libraries(core-minifi ${CMAKE_DL_LIBS} ${LIBMINIFI_LIBRARIES})


add_library(minifi STATIC ${PROCESSOR_SOURCES})
target_link_libraries(minifi core-minifi)
if (WIN32)
	set_target_properties(minifi PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
	set_target_properties(minifi PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:core-minifi")
endif()


SET (LIBMINIFI core-minifi PARENT_SCOPE)

if (ENABLE_PYTHON AND NOT STATIC_BUILD)
	add_library(core-minifi-shared SHARED ${SOURCES})
	target_link_libraries(core-minifi-shared ${CMAKE_DL_LIBS} ${LIBMINIFI_LIBRARIES})

	add_library(minifi-shared SHARED ${PROCESSOR_SOURCES})
	target_link_libraries(minifi-shared core-minifi-shared)
	if (WIN32)
		set_target_properties(minifi-shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
		set_target_properties(minifi-shared PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:core-minifi-shared")
	endif()
endif(ENABLE_PYTHON AND NOT STATIC_BUILD)
