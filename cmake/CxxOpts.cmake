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

include(FetchContent)

FetchContent_Declare(cxxopts_src
    URL      https://github.com/jarro2783/cxxopts/archive/refs/tags/v2.2.1.tar.gz
    URL_HASH SHA256=984aa3c8917d649b14d7f6277104ce38dd142ce378a9198ec926f03302399681
)
FetchContent_GetProperties(cxxopts_src)
if (NOT cxxopts_src_POPULATED)
    FetchContent_Populate(cxxopts_src)
    set(CXXOPTS_INCLUDE_DIR "${cxxopts_src_SOURCE_DIR}/include" CACHE STRING "" FORCE)
    add_library(cxxopts INTERFACE)
    add_library(cxxopts::cxxopts ALIAS cxxopts)
    target_sources(cxxopts INTERFACE ${CXXOPTS_INCLUDE_DIR}/cxxopts.hpp)
    target_include_directories(cxxopts SYSTEM INTERFACE ${CXXOPTS_INCLUDE_DIR})
    target_compile_features(cxxopts INTERFACE cxx_std_11)
endif()
