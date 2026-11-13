# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/llamacpp/all/patches/mtmd-fix.patch")

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

add_library(llama-cpp::llama ALIAS llama)
add_library(llama-cpp::common ALIAS llama-common)
add_library(llama-cpp::mtmd ALIAS mtmd)

set(LLAMACPP_INCLUDE_DIRS
    "${llamacpp_SOURCE_DIR}/include"
    "${llamacpp_SOURCE_DIR}/ggml/include"
    "${llamacpp_SOURCE_DIR}/tools"
    "${llamacpp_SOURCE_DIR}/common"
    "${llamacpp_SOURCE_DIR}/vendor"
    CACHE STRING "" FORCE
)
