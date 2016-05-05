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

set(PROJECT "nifi-minifi-cpp")
set(VERSION "0.1.0")

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
# https://cmake.org/cmake/help/v3.0/policy/CMP0048.html#policy:CMP0048
cmake_policy(SET CMP0048 NEW)

project(${PROJECT}
        VERSION ${VERSION})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Provide custom modules for the project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#file(GLOB SOURCES "libminifi/src/*.cpp")
add_subdirectory(thirdparty/yaml-cpp-yaml-cpp-0.5.3)
add_subdirectory(libminifi)
add_subdirectory(main)

# Generate source assembly
set(ASSEMBLY_BASE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}-source")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.bzr/;~$;${CPACK_SOURCE_IGNORE_FILES}")

# Generate binary assembly
install(FILES conf/minifi.properties
        DESTINATION conf
        COMPONENT bin)

install(PROGRAMS bin/minifi.sh
        DESTINATION bin
        COMPONENT bin)

install(FILES LICENSE README.md NOTICE
        DESTINATION .
        COMPONENT bin)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apache NiFi MiNiFi C++ version ${VERSION}")
set(CPACK_PACKAGE_VENDOR "Apache NiFi")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}")
set(CPACK_BINARY_TGZ, "ON")

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL bin)

include(CPack)
