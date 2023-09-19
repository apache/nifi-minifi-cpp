#
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
#
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL absl-propagate-cxx)
set(ABSL_ENABLE_INSTALL ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE STRING "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/abseil/rename-crc32.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
        absl
        URL      https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.0.tar.gz
        URL_HASH SHA256=59d2976af9d6ecf001a81a35749a6e551a335b949d34918cfade07737b9d93c5
        PATCH_COMMAND "${PC}"
)
FetchContent_MakeAvailable(absl)
