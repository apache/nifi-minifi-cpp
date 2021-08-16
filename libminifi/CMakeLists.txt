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

cmake_minimum_required (VERSION 3.16)
cmake_policy(SET CMP0096 NEW) # policy to preserve the leading zeros in PROJECT_VERSION_{MAJOR,MINOR,PATCH,TWEAK}
project(nifi-libcore-minifi VERSION 0.11.0)
set(PROJECT_NAME "nifi-libcore-minifi")

if (WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()
if (IOS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-abi-version=2 -fobjc-arc -std=gnu++2a -stdlib=libc++ -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-abi-version=2 -fobjc-arc -isysroot ${CMAKE_OSX_SYSROOT} -DIOS")
else ()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DYAML_SUPPORT")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DYAML_SUPPORT")
endif()

include(CppVersion)
set_cpp_version()

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
	set(TLS_SOURCES "src/utils/tls/*.cpp" "src/io/tls/*.cpp")
endif()

file(GLOB SOURCES "src/agent/agent_docs.cpp" "src/properties/*.cpp" "src/utils/file/*.cpp" "src/sitetosite/*.cpp"  "src/core/logging/*.cpp" "src/core/logging/internal/*.cpp" "src/core/state/*.cpp" "src/core/state/nodes/*.cpp" "src/c2/protocols/*.cpp" "src/c2/triggers/*.cpp" "src/c2/*.cpp" "src/io/*.cpp" ${SOCKET_SOURCES} ${TLS_SOURCES} "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/controllers/keyvalue/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp" "src/core/extension/*.cpp" "src/serialization/*.cpp" "src/provenance/*.cpp" "src/utils/*.cpp" "src/utils/crypto/*.cpp" "src/utils/crypto/ciphers/*.cpp" "src/*.cpp")
# manually add this as it might not yet be present when this executes
list(APPEND SOURCES "src/agent/agent_version.cpp")

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


add_library(core-minifi SHARED ${SOURCES})
target_compile_definitions(core-minifi PRIVATE "LIBMINIFI=1")
if(WIN32)
	set_target_properties(core-minifi PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
	add_dependencies(core-minifi message-strings)
	target_include_directories(core-minifi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
	set_target_properties(core-minifi PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
	set_target_properties(core-minifi PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(NOT EXCLUDE_BOOST)
  find_package(Boost COMPONENTS system filesystem)
  if(Boost_FOUND)
    add_definitions(-DUSE_BOOST)
    target_include_directories(core-minifi PRIVATE "${Boost_INCLUDE_DIRS}")
    list(APPEND LIBMINIFI_LIBRARIES ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY})
  endif()
endif()

include(RangeV3)
list(APPEND LIBMINIFI_LIBRARIES yaml-cpp ZLIB::ZLIB concurrentqueue RapidJSON spdlog cron Threads::Threads gsl-lite libsodium range-v3)
if(NOT WIN32)
	list(APPEND LIBMINIFI_LIBRARIES OSSP::libuuid++)
endif()
if (NOT OPENSSL_OFF)
	list(APPEND LIBMINIFI_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
endif()
target_link_libraries(core-minifi ${CMAKE_DL_LIBS} ${LIBMINIFI_LIBRARIES})

SET (LIBMINIFI core-minifi PARENT_SCOPE)

if (WIN32)
	install(TARGETS core-minifi ARCHIVE DESTINATION bin COMPONENT bin)
	install(TARGETS core-minifi RUNTIME DESTINATION bin COMPONENT bin)
else()
	install(TARGETS core-minifi LIBRARY DESTINATION bin COMPONENT bin)
endif()
