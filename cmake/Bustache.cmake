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
include(GetFmt)
get_fmt()

set(BUSTACHE_USE_FMT ON CACHE STRING "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/bustache/add-append.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/bustache/fix-deprecated-literal-operator.patch")

set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

FetchContent_Declare(Bustache
        GIT_REPOSITORY  https://github.com/jamboree/bustache.git
        GIT_TAG         47096caa8e1f9f7ebe34e3a022dbb822c174011d
        PATCH_COMMAND   "${PC}"
        SYSTEM
)
FetchContent_MakeAvailable(Bustache)
