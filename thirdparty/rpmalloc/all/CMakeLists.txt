# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.15)
project(rpmalloc C)

# rpmalloc does not ship a CMake build; this mirrors the bundled RpMalloc.cmake target.
add_library(rpmalloc STATIC rpmalloc/rpmalloc.c)
target_include_directories(rpmalloc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rpmalloc>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(rpmalloc PRIVATE ENABLE_OVERRIDE=1 ENABLE_PRELOAD=1)

include(GNUInstallDirs)
install(TARGETS rpmalloc ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES rpmalloc/rpmalloc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
