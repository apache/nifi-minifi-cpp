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

FetchContent_Declare(pybind11_src
    URL      https://github.com/pybind/pybind11/archive/refs/tags/v2.7.0.tar.gz
    URL_HASH SHA256=6cd73b3d0bf3daf415b5f9b87ca8817cc2e2b64c275d65f9500250f9fee1677e
)
FetchContent_GetProperties(pybind11_src)
if (NOT pybind11_src_POPULATED)
    FetchContent_Populate(pybind11_src)
    set(PYBIND11_INCLUDE_DIR "${pybind11_src_SOURCE_DIR}/include" CACHE STRING "" FORCE)
    add_library(pybind11 INTERFACE IMPORTED)
    target_sources(pybind11 INTERFACE ${PYBIND11_INCLUDE_DIR}/pybind11/pybind11.h)
    target_include_directories(pybind11 SYSTEM INTERFACE ${PYBIND11_INCLUDE_DIR})
    target_compile_features(pybind11 INTERFACE cxx_std_11)
endif()
