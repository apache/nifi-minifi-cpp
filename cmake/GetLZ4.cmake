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

if(MINIFI_LZ4_SOURCE STREQUAL "CONAN")
    message("Using Conan to install lz4")
    find_package(lz4 REQUIRED)
    if(NOT TARGET lz4::lz4)
        add_library(lz4::lz4 ALIAS LZ4::liblz4_static)
    endif()
    if(NOT TARGET LZ4::LZ4)
        add_library(LZ4::LZ4 ALIAS lz4::lz4)
    endif()
    set(LZ4_LIBRARIES "${lz4_LIBRARIES}" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIRS "${lz4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
elseif(MINIFI_LZ4_SOURCE STREQUAL "BUILD")
    message("Using CMake to build lz4 from source")
    include(LZ4)
endif()
