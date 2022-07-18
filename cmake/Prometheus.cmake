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

set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(USE_THIRDPARTY_LIBRARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION ON CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/prometheus-cpp/remove-find_package.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    prometheus-cpp
    URL "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.0.1.tar.gz"
    URL_HASH "SHA256=593e028d401d3298eada804d252bc38d8cab3ea1c9e88bcd72095281f85e6d16"
    PATCH_COMMAND "${PC}"
)

FetchContent_MakeAvailable(prometheus-cpp)
