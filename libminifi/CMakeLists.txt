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

cmake_minimum_required (VERSION 3.24)
cmake_policy(SET CMP0096 NEW) # policy to preserve the leading zeros in PROJECT_VERSION_{MAJOR,MINOR,PATCH,TWEAK}
cmake_policy(SET CMP0135 NEW) # policy to set the timestamps of extracted contents to the time of extraction
project(nifi-libcore-minifi VERSION 0.15.0)
set(PROJECT_NAME "nifi-libcore-minifi")

if (WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

include(CppVersion)
set_cpp_version()

include_directories(include)

if(WIN32)
    include_directories(opsys/win)
    set(SOCKET_SOURCES "src/io/win/*.cpp")
else()
    include_directories(opsys/posix)
    set(SOCKET_SOURCES "src/io/posix/*.cpp")
endif()

if (MINIFI_OPENSSL)
    set(TLS_SOURCES "src/utils/tls/*.cpp" "src/io/tls/*.cpp")
endif()

file(GLOB SOURCES "src/agent/*.cpp" "src/properties/*.cpp" "src/utils/file/*.cpp" "src/sitetosite/*.cpp"  "src/core/logging/*.cpp" "src/core/logging/internal/*.cpp" "src/core/logging/alert/*.cpp" "src/core/state/*.cpp" "src/core/state/nodes/*.cpp" "src/c2/protocols/*.cpp" "src/c2/triggers/*.cpp" "src/c2/*.cpp" "src/io/*.cpp" ${SOCKET_SOURCES} ${TLS_SOURCES} "src/core/controller/*.cpp" "src/controllers/*.cpp" "src/controllers/keyvalue/*.cpp" "src/core/*.cpp"  "src/core/repository/*.cpp" "src/core/flow/*.cpp" "src/core/json/*.cpp" "src/core/yaml/*.cpp" "src/core/reporting/*.cpp" "src/core/extension/*.cpp" "src/serialization/*.cpp" "src/provenance/*.cpp" "src/utils/*.cpp" "src/utils/crypto/*.cpp" "src/utils/crypto/ciphers/*.cpp" "src/utils/crypto/property_encryption/*.cpp" "src/*.cpp" "src/utils/net/*.cpp")
# manually add this as it might not yet be present when this executes
list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/agent_version.cpp")

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


add_minifi_library(core-minifi SHARED ${SOURCES})
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

# Recursively get all LIBMINIFI header directories, store into generated include_dirs.cmake using python
find_package(Python3 COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/python/get_include_dirs.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/include" # LIBMINIFI include directory
    "${CMAKE_CURRENT_BINARY_DIR}/include_dirs.cmake" # generated LIBMINIFI include_dirs.cmake file
    "LIBMINIFI_INCLUDE_DIRS" # CMake variable name for LIBMINIFI INCLUDE_DIRS
    )

# Include the generated cmake file to set the LIBMINIFI_INCLUDE_DIRS variable
include("${CMAKE_CURRENT_BINARY_DIR}/include_dirs.cmake")

message("LIBMINIFI_INCLUDE_DIRS = ${LIBMINIFI_INCLUDE_DIRS}")
target_include_directories(core-minifi PUBLIC ${LIBMINIFI_INCLUDE_DIRS})

# TODO (JG): Analyze the 3rd party lib dependencies (thirdparty, extensions, etc) on libminifi and replace them with the appropriate conan packages
include(RangeV3)
include(Asio)
include(MagicEnum)
list(APPEND LIBMINIFI_LIBRARIES yaml-cpp::yaml-cpp ZLIB::ZLIB concurrentqueue RapidJSON spdlog::spdlog Threads::Threads gsl::gsl-lite libsodium::libsodium range-v3::range-v3 nonstd::expected-lite date::date date::date-tz asio::asio magic_enum::magic_enum)
if(NOT WIN32)
    list(APPEND LIBMINIFI_LIBRARIES OSSP::libuuid++)
endif()
if (MINIFI_OPENSSL)
    list(APPEND LIBMINIFI_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    list(APPEND LIBMINIFI_LIBRARIES stdc++fs)
endif()
target_link_libraries(core-minifi ${CMAKE_DL_LIBS} ${LIBMINIFI_LIBRARIES})
if (UNIX AND (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7"))
    target_link_libraries(core-minifi "-latomic")
endif()

SET (LIBMINIFI core-minifi PARENT_SCOPE)

if (WIN32)
    install(TARGETS core-minifi ARCHIVE DESTINATION bin COMPONENT bin)
    install(TARGETS core-minifi RUNTIME DESTINATION bin COMPONENT bin)
else()
    install(TARGETS core-minifi LIBRARY DESTINATION bin COMPONENT bin)
endif()
