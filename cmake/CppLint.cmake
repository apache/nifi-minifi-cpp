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

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/main_args.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    cpplint
    URL           https://github.com/cpplint/cpplint/archive/refs/tags/2.0.2.tar.gz
    URL_HASH      SHA256=fc6d0cd40f934b58e8e0bb5eb5f1f2b651880b5fbc0e93a54e6fb6503f733b3d
    PATCH_COMMAND ${PC}
    SOURCE_DIR    ${CMAKE_BINARY_DIR}/cpplint
    SOURCE_SUBDIR does-not-exist   # only download the source
)
FetchContent_MakeAvailable(cpplint)

set(CPPLINT_DIR ${CMAKE_BINARY_DIR}/cpplint)
file(COPY "${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.py" DESTINATION "${CPPLINT_DIR}")
