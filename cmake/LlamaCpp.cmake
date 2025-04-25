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

set(BUILD_SHARED_LIBS "OFF" CACHE STRING "" FORCE)
set(LLAMA_BUILD_TESTS "OFF" CACHE STRING "" FORCE)
set(LLAMA_BUILD_EXAMPLES "OFF" CACHE STRING "" FORCE)
set(LLAMA_BUILD_SERVER "OFF" CACHE STRING "" FORCE)
set(GGML_OPENMP "OFF" CACHE STRING "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/llamacpp/metal.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/llamacpp/lu8_macro_fix.patch")  # https://github.com/ggml-org/llama.cpp/issues/12740
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

FetchContent_Declare(llamacpp
        URL https://github.com/ggerganov/llama.cpp/archive/refs/tags/b5038.tar.gz
        URL_HASH SHA256=5e81c3badc181ed3b7a6ab6bda2abedc80c52527e3c079c7afff4c09f4843564
        PATCH_COMMAND "${PC}"
)

FetchContent_MakeAvailable(llamacpp)

set(LLAMACPP_INCLUDE_DIRS
    "${llamacpp_SOURCE_DIR}/include"
    "${llamacpp_SOURCE_DIR}/ggml/include"
    CACHE STRING "" FORCE
)
