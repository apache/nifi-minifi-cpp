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
	include_directories("thirdparty/uuid/include/win32/")
else()
	include_directories("thirdparty/uuid/include/posix")
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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-reorder")
endif()

include_directories(../thirdparty/spdlog-20170710/include)
include_directories(../thirdparty/yaml-cpp-yaml-cpp-20171024/include)

include_directories(../thirdparty/rapidjson-1.1.0/include)
include_directories(../thirdparty/concurrentqueue/)
include_directories(include)


if(WIN32)
include_directories(opsys/win)
set(SOCKET_SOURCES "src/io/win/*.cpp")
else()
include_directories(opsys/posix)
set(SOCKET_SOURCES "src/io/posix/*.cpp")
endif()

find_package(OpenSSL)
if (OPENSSL_FOUND)
	set(TLS_SOURCES "src/io/tls/*.cpp")
endif(OPENSSL_FOUND)
file(GLOB SOURCES  "src/sitetosite/*.cpp"  "src/core/logging/*.cpp"  "src/core/state/*.cpp" "src/core/state/nodes/*.cpp" "src/c2/protocols/*.cpp" "src/c2/triggers/*.cpp" "src/c2/*.cpp" "src/io/*.cpp" ${SOCKET_SOURCES} ${TLS_SOURCES} "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp"  "src/provenance/*.cpp" "src/utils/*.cpp" "src/*.cpp")

file(GLOB PROCESSOR_SOURCES  "src/processors/*.cpp" )


file(GLOB SPD_SOURCES "../thirdparty/spdlog-20170710/include/spdlog/*")

# Workaround the limitations of having a
# header only library
add_library(spdlog STATIC ${SPD_SOURCES})
add_library(core-minifi STATIC ${SOURCES})
target_link_libraries(core-minifi ${CMAKE_DL_LIBS} ${UUID_LIBRARIES} yaml-cpp)

#target_link_libraries(core-minifi  PRIVATE bsdiff )

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

target_link_libraries(core-minifi minifi-expression-language-extensions)
target_link_libraries (core-minifi ${ZLIB_LIBRARIES})
set_target_properties(core-minifi PROPERTIES LINK_FLAGS "/WHOLEARCHIVE")



# Include OpenSSL

if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
	#target_link_libraries (core-minifi ${OPENSSL_LIBRARIES})
	if (WIN32)
		foreach(OPENSSL_LIB ${OPENSSL_LIBRARIES})
			if (OPENSSL_LIB MATCHES "\\.lib$" OR OPENSSL_LIB MATCHES "\\.dll$" )
			message( "Including ${OPENSSL_LIB}")
			target_link_libraries (core-minifi ${OPENSSL_LIB})	
			set_target_properties(core-minifi PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:${OPENSSL_LIB}")
			endif()
		endforeach()
	else()
		target_link_libraries (core-minifi ${OPENSSL_LIBRARIES})
	endif()

endif (OPENSSL_FOUND)

add_library(minifi STATIC ${PROCESSOR_SOURCES})


target_link_libraries(minifi core-minifi)
if (WIN32)
set_target_properties(minifi PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
set_target_properties(minifi PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:core-minifi")
endif()


SET (LIBMINIFI core-minifi PARENT_SCOPE)

if (ENABLE_PYTHON AND NOT STATIC_BUILD)
#### shared

add_library(core-minifi-shared SHARED ${SOURCES})
if (APPLE)
	target_link_libraries(core-minifi-shared ${CMAKE_DL_LIBS} yaml-cpp)
else()
	target_link_libraries(core-minifi-shared ${CMAKE_DL_LIBS} uuid-shared yaml-cpp)
endif()

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

target_link_libraries(core-minifi-shared minifi-expression-language-extensions)
target_link_libraries (core-minifi-shared ${ZLIB_LIBRARIES})
if (WIN32)
set_target_properties(core-minifi-shared PROPERTIES LINK_FLAGS "/WHOLEARCHIVE")
endif()


# Include OpenSSL

if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
	target_link_libraries (core-minifi-shared ${OPENSSL_LIBRARIES})
endif (OPENSSL_FOUND)

add_library(minifi-shared SHARED ${PROCESSOR_SOURCES})

target_link_libraries(minifi-shared core-minifi-shared)
if (WIN32)
set_target_properties(minifi-shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
set_target_properties(minifi-shared PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:core-minifi-shared")
endif()


set_property(TARGET core-minifi-shared PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET minifi-shared PROPERTY POSITION_INDEPENDENT_CODE ON)
#endif()
endif(ENABLE_PYTHON AND NOT STATIC_BUILD)