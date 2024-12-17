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

function(use_bundled_llamacpp SOURCE_DIR BINARY_DIR)
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/llamacpp/metal.patch")

    set(BYPRODUCTS
        "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libllama.a"
        "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml.a"
        "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-base.a"
        "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-cpu.a"
        "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-blas.a"
    )

    if (APPLE)
        list(APPEND BYPRODUCTS
            "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-metal.a"
        )
    endif()

    set(LLAMACPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
        "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/llamacpp-install"
        -DBUILD_SHARED_LIBS=OFF
        -DLLAMA_BUILD_TESTS=OFF
        -DLLAMA_BUILD_EXAMPLES=OFF
        -DLLAMA_BUILD_SERVER=OFF
    )

    append_third_party_passthrough_args(LLAMACPP_CMAKE_ARGS "${LLAMACPP_CMAKE_ARGS}")

    ExternalProject_Add(
        llamacpp-external
        URL https://github.com/ggerganov/llama.cpp/archive/refs/tags/b4174.tar.gz
        URL_HASH "SHA256=571ef4c645784db56a482c453e9090d737913d25a178a00d5e0afd8d434afae0"
        SOURCE_DIR "${BINARY_DIR}/thirdparty/llamacpp-src"
        CMAKE_ARGS ${LLAMACPP_CMAKE_ARGS}
        BUILD_BYPRODUCTS ${BYPRODUCTS}
        PATCH_COMMAND ${PC}
        EXCLUDE_FROM_ALL TRUE
    )

#    set(LLAMACPP_FOUND "YES" CACHE STRING "" FORCE)
    set(LLAMACPP_INCLUDE_DIR "${BINARY_DIR}/thirdparty/llamacpp-install/include" CACHE STRING "" FORCE)
#    set(LLAMACPP_LIBRARIES "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libllama.a;${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml.a;${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-base.a" CACHE STRING "" FORCE)

    add_library(llamacpp INTERFACE)

    add_library(LlamaCpp::llama STATIC IMPORTED)
    set_target_properties(LlamaCpp::llama PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libllama.a")
    add_dependencies(LlamaCpp::llama llamacpp-external)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::llama)

    add_library(LlamaCpp::ggml STATIC IMPORTED)
    set_target_properties(LlamaCpp::ggml PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml.a")
    add_dependencies(LlamaCpp::ggml llamacpp-external)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml)

    add_library(LlamaCpp::ggml-base STATIC IMPORTED)
    set_target_properties(LlamaCpp::ggml-base PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-base.a")
    add_dependencies(LlamaCpp::ggml-base llamacpp-external)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml-base)

    # backends
    add_library(LlamaCpp::ggml-cpu STATIC IMPORTED)
    set_target_properties(LlamaCpp::ggml-cpu PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-cpu.a")
    add_dependencies(LlamaCpp::ggml-cpu llamacpp-external)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml-cpu)

    add_library(LlamaCpp::ggml-blas STATIC IMPORTED)
    set_target_properties(LlamaCpp::ggml-blas PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-blas.a")
    add_dependencies(LlamaCpp::ggml-blas llamacpp-external)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml-blas)

    if (APPLE)
        add_library(LlamaCpp::ggml-metal STATIC IMPORTED)
        set_target_properties(LlamaCpp::ggml-metal PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml-metal.a")
        add_dependencies(LlamaCpp::ggml-metal llamacpp-external)
        target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml-metal)
    endif()

    file(MAKE_DIRECTORY ${LLAMACPP_INCLUDE_DIR})
    target_include_directories(llamacpp INTERFACE ${LLAMACPP_INCLUDE_DIR})

    if (APPLE)
        target_link_libraries(llamacpp INTERFACE "-framework Metal" "-framework CoreFoundation" "-framework Foundation" "-framework Accelerate")
    endif()
endfunction()