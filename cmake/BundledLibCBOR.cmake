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

function(use_bundled_libcbor SOURCE_DIR BINARY_DIR)
    set(CBOR_SOURCE_DIR "${BINARY_DIR}/thirdparty/libcbor-src")
    set(CBOR_BINARY_DIR "${BINARY_DIR}/thirdparty/libcbor-install")

    if(MSVC)
      set(CBOR_RESTRICT_SPECIFIER "")
    elseif(APPLE)
      set(CBOR_RESTRICT_SPECIFIER "restrict")
    else()
      set(CBOR_RESTRICT_SPECIFIER "__restrict__")
    endif()

    # Define byproducts
    if (WIN32)
      set(LIB_CBOR "${CBOR_BINARY_DIR}/src/${CMAKE_BUILD_TYPE}/cbor.lib" CACHE STRING "" FORCE)
    else()
      set(LIB_CBOR "${CBOR_BINARY_DIR}/src/libcbor.a" CACHE STRING "" FORCE)
    endif()

    set(CBOR_BYPRODUCT "${LIB_CBOR}")
    set(LIBCBOR_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
      "-DCBOR_CUSTOM_ALLOC=ON"
      "-DCMAKE_BUILD_TYPE=Release")

    # Build project
    ExternalProject_Add(
      libcbor-external
      GIT_REPOSITORY "https://github.com/PJK/libcbor.git"
      GIT_TAG "v0.6.1"
      SOURCE_DIR "${CBOR_SOURCE_DIR}"
      BINARY_DIR "${CBOR_BINARY_DIR}"
      CMAKE_ARGS "${LIBCBOR_CMAKE_ARGS}"
      INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "skip install step"
      BUILD_BYPRODUCTS "${CBOR_BYPRODUCT}"
      EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(CBOR_INCLUDE_DIR "${CBOR_SOURCE_DIR}/src" "${CBOR_BINARY_DIR}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(cbor STATIC IMPORTED)
    add_dependencies(cbor libcbor-external)    
    add_library(libcbor INTERFACE)
    set_target_properties(cbor PROPERTIES IMPORTED_LOCATION "${CBOR_BYPRODUCT}"
                          INTERFACE_COMPILE_DEFINITIONS "restrict=${CBOR_RESTRICT_SPECIFIER}")

    target_include_directories(libcbor INTERFACE "${CBOR_INCLUDE_DIR}")
    target_link_libraries(libcbor INTERFACE cbor)
endfunction(use_bundled_libcbor)
