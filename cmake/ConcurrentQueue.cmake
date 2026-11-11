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

FetchContent_Declare(
    concurrentqueue
    URL           https://github.com/cameron314/concurrentqueue/archive/refs/tags/v1.0.5.tar.gz
    URL_HASH      SHA256=4d6368a27492d86011fde5ca0cf386dce7c49cd425aa3d9b063ca6ec373a6ef3
    SOURCE_DIR    ${CMAKE_BINARY_DIR}/concurrentqueue_include/moodycamel
    SOURCE_SUBDIR does-not-exist   # don't run upstream CMake; we define the target
)
FetchContent_MakeAvailable(concurrentqueue)

if(NOT TARGET concurrentqueue::concurrentqueue)
    add_library(concurrentqueue INTERFACE)
    target_include_directories(concurrentqueue SYSTEM INTERFACE
        ${CMAKE_BINARY_DIR}/concurrentqueue_include)
    add_library(concurrentqueue::concurrentqueue ALIAS concurrentqueue)
endif()
