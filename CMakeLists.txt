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

cmake_minimum_required(VERSION 2.6)
project(nifi-minifi-cpp)
set(PROJECT_NAME "nifi-minifi-cpp")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)
option(test "Build all tests." ON)

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
# https://cmake.org/cmake/help/v3.0/policy/CMP0048.html#policy:CMP0048
IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11 " COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x " COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable asan in DEBUG for compatibility with civet
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

# Search for threads
find_package(Threads REQUIRED)

# Set the right openssl root path
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
else()
set(OPENSSL_ROOT_DIR "/usr/lib/x86_64-linux-gnu")
endif()

# Include OpenSSL
find_package (OpenSSL REQUIRED)
if (OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
else ()
    message( FATAL_ERROR "OpenSSL was not found. Please install OpenSSL" )
endif (OPENSSL_FOUND)

# Provide custom modules for the project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include LevelDB
find_package (Leveldb REQUIRED)
if (LEVELDB_FOUND)
        include_directories(${LEVELDB_INCLUDE_DIRS})
else ()
    message( FATAL_ERROR "LevelDB was not found. Please install LevelDB" )
endif (LEVELDB_FOUND)
find_package(UUID REQUIRED)
file(GLOB SPD_SOURCES "include/spdlog/*")

add_subdirectory(thirdparty/yaml-cpp-yaml-cpp-0.5.3)
add_subdirectory(thirdparty/jsoncpp)
set(CIVETWEB_ENABLE_CXX ON CACHE BOOL "Enable civet C++ library")
add_subdirectory(thirdparty/civetweb-1.9.1)
add_subdirectory(libminifi)
add_subdirectory(main)


# Generate source assembly
set(ASSEMBLY_BASE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}-source")
set(CPACK_SOURCE_IGNORE_FILES "/build/;~$;${CPACK_SOURCE_IGNORE_FILES};${CMAKE_SOURCE_DIR}/.git/;${CMAKE_SOURCE_DIR}/.idea/;${CMAKE_SOURCE_DIR}/cmake-build-debug/")

# Generate binary assembly
install(FILES conf/minifi.properties conf/config.yml
        DESTINATION conf
        COMPONENT bin)

install(PROGRAMS bin/minifi.sh
        DESTINATION bin
        COMPONENT bin)

install(FILES LICENSE README.md NOTICE
        DESTINATION .
        COMPONENT bin)

set(CPACK_GENERATOR "TGZ")
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY 1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apache NiFi MiNiFi C++ version ${VERSION}")
set(CPACK_PACKAGE_VENDOR "Apache NiFi")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}")
set(CPACK_BINARY_TGZ, "ON")

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL bin)

include(CPack)

enable_testing(test)
    file(GLOB LIBMINIFI_TEST_SOURCES "libminifi/test/unit/*.cpp")
    add_executable(tests ${LIBMINIFI_TEST_SOURCES} ${SPD_SOURCES})
    target_include_directories(tests PRIVATE BEFORE "thirdparty/catch")
    target_include_directories(tests PRIVATE BEFORE "thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(tests PRIVATE BEFORE "thirdparty/jsoncpp/include")
    target_include_directories(tests PRIVATE BEFORE ${LEVELDB_INCLUDE_DIRS})
    target_include_directories(tests PRIVATE BEFORE "include")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/core")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/core/repository")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/io")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/utils")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/processors")
    target_include_directories(tests PRIVATE BEFORE "libminifi/include/provenance")
    target_link_libraries(tests ${CMAKE_THREAD_LIBS_INIT} ${UUID_LIBRARIES} ${LEVELDB_LIBRARIES} ${OPENSSL_LIBRARIES} minifi yaml-cpp c-library civetweb-cpp jsoncpp_lib_static)
    add_test(NAME LibMinifiTests COMMAND tests)
    
    file(GLOB LIBMINIFI_TEST_EXECUTE_PROCESS "libminifi/test/TestExecuteProcess.cpp")
    add_executable(testExecuteProcess ${LIBMINIFI_TEST_EXECUTE_PROCESS} ${SPD_SOURCES})
    target_include_directories(testExecuteProcess PRIVATE BEFORE "thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(testExecuteProcess PRIVATE BEFORE ${LEVELDB_INCLUDE_DIRS})
    target_include_directories(testExecuteProcess PRIVATE BEFORE "include")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/core")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/core/repository")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/io")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/utils")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/processors")
    target_include_directories(testExecuteProcess PRIVATE BEFORE "libminifi/include/provenance")
    target_link_libraries(testExecuteProcess ${CMAKE_THREAD_LIBS_INIT} ${UUID_LIBRARIES} ${LEVELDB_LIBRARIES} ${OPENSSL_LIBRARIES} minifi yaml-cpp c-library civetweb-cpp)
    add_test(NAME ExecuteProcess COMMAND testExecuteProcess)

# Create a custom build target called "docker" that will invoke DockerBuild.sh and create the NiFi-MiNiFi-CPP Docker image
add_custom_target(
    docker
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh 1000 1000 ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} minificppsource ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

# Create a custom build target that will run the linter.
add_custom_target(
    linter
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh ${CMAKE_SOURCE_DIR}/libminifi/include/ ${CMAKE_SOURCE_DIR}/libminifi/src/ )


FIND_PACKAGE(Doxygen)

if(DOXYGEN_FOUND)
if(EXISTS ${DOXYGEN_EXECUTABLE})

MESSAGE("Creating API Documentation ${DOXYGEN_EXECUTABLE}")
SET(DOXYGEN_INPUT "../docs/Doxyfile")
SET(DOXYGEN_OUTPUT "../docs")

ADD_CUSTOM_COMMAND(
  OUTPUT ${DOXYGEN_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E echo_append "Building LibMiNiFi API Documentation..."
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT}
  COMMAND ${CMAKE_COMMAND} -E echo "Done."
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${DOXYGEN_INPUT}
  )

ADD_CUSTOM_TARGET(docs ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT})
else()
ADD_CUSTOM_TARGET(docs echo "Doxygen binary does not exist. Cannot create documentation... Please install Doxygen or verify correct installation.")
endif()
else()
ADD_CUSTOM_TARGET(docs echo "Doxygen does not exist. Please install it. Cannot create documentation...")
endif(DOXYGEN_FOUND)
