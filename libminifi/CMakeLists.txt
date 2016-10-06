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
# Unless qrequired by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

cmake_minimum_required (VERSION 2.6)

set(PROJECT "apache-nifi-minifi-cpp")
set(VERSION "0.1.0")

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
# https://cmake.org/cmake/help/v3.0/policy/CMP0048.html#policy:CMP0048
cmake_policy(SET CMP0048 NEW)

project(${PROJECT}
        VERSION ${VERSION})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../include)
include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_library(spdlog INTERFACE)
add_library(minifi STATIC ${SOURCES})

# Include libxml2
find_package (LibXml2)
if (LIBXML2_FOUND)
    include_directories(${LIBXML2_INCLUDE_DIR})
    target_link_libraries (minifi ${LIBXML2_LIBRARIES})
else ()
    # Build from our local version
endif (LIBXML2_FOUND)