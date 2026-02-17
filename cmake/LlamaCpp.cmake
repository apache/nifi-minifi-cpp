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
set(LLAMA_BUILD_TOOLS "ON" CACHE STRING "" FORCE)
set(GGML_OPENMP "OFF" CACHE STRING "" FORCE)
set(GGML_METAL "OFF" CACHE STRING "" FORCE)
set(GGML_BLAS "OFF" CACHE STRING "" FORCE)
if (PORTABLE)
    set(GGML_NATIVE "OFF" CACHE STRING "" FORCE)
else()
    set(GGML_NATIVE "ON" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(llamacpp
        URL https://github.com/ggml-org/llama.cpp/archive/refs/tags/b7836.tar.gz
        URL_HASH SHA256=3d384e7e8b3bc3cd31abddedf684a6e201405c1d932cafb3c4a5277d872b0614
        SYSTEM
)

FetchContent_MakeAvailable(llamacpp)

set(LLAMACPP_INCLUDE_DIRS
    "${llamacpp_SOURCE_DIR}/include"
    "${llamacpp_SOURCE_DIR}/ggml/include"
    "${llamacpp_SOURCE_DIR}/tools"
    CACHE STRING "" FORCE
)
