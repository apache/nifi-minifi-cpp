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
set(LLAMA_BUILD_COMMON "ON" CACHE STRING "" FORCE)
set(GGML_OPENMP "OFF" CACHE STRING "" FORCE)
set(GGML_METAL "OFF" CACHE STRING "" FORCE)
set(GGML_BLAS "OFF" CACHE STRING "" FORCE)
if (PORTABLE)
    set(GGML_NATIVE "OFF" CACHE STRING "" FORCE)
else()
    set(GGML_NATIVE "ON" CACHE STRING "" FORCE)
endif()

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/llamacpp/mtmd-fix.patch")

set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\")")


FetchContent_Declare(llamacpp
    URL https://github.com/ggml-org/llama.cpp/archive/refs/tags/b8944.tar.gz
    URL_HASH SHA256=ca231c8aca086f56bad3ed371f6dc5b01e971e812a8ddf67564f087390c0e781
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(llamacpp)

if(MSVC AND TARGET llama)
    target_compile_options(llama PRIVATE /Zc:__cplusplus)
endif()

set(LLAMACPP_INCLUDE_DIRS
    "${llamacpp_SOURCE_DIR}/include"
    "${llamacpp_SOURCE_DIR}/ggml/include"
    "${llamacpp_SOURCE_DIR}/tools"
    "${llamacpp_SOURCE_DIR}/common"
    "${llamacpp_SOURCE_DIR}/vendor"
    CACHE STRING "" FORCE
)
