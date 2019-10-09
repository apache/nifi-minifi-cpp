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

function(use_bundled_jemalloc SOURCE_DIR BINARY_DIR)
    message("Using bundled jemalloc")

    # Define byproducts
    set(BYPRODUCT "lib/libjemalloc.a")

    # Build project
    ExternalProject_Add(
            jemalloc-external
            GIT_REPOSITORY "https://github.com/jemalloc/jemalloc.git"
            GIT_TAG "5.1.0"
            PREFIX "${BINARY_DIR}/thirdparty/jemalloc"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${BINARY_DIR}/thirdparty/jemalloc-src"
            BUILD_COMMAND make
            CMAKE_COMMAND ""
            UPDATE_COMMAND ""
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/jemalloc-src/${BYPRODUCT}"
            INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Skipping install step."
            CONFIGURE_COMMAND ""
            PATCH_COMMAND ./autogen.sh && ./configure
            STEP_TARGETS build
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(JEMALLOC_FOUND "YES" CACHE STRING "" FORCE)
    set(JEMALLOC_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/jemalloc-src/include" CACHE STRING "" FORCE)
    set(JEMALLOC_LIBRARY "${BINARY_DIR}/thirdparty/jemalloc-src/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(JEMALLOC_LIBRARIES "${JEMALLOC_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(JeMalloc::JeMalloc STATIC IMPORTED)
    set_target_properties(JeMalloc::JeMalloc PROPERTIES IMPORTED_LOCATION "${JEMALLOC_LIBRARY}")
    add_dependencies(JeMalloc::JeMalloc jemalloc-external)
    file(MAKE_DIRECTORY ${JEMALLOC_INCLUDE_DIRS})
    set_property(TARGET JeMalloc::JeMalloc APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${JEMALLOC_INCLUDE_DIRS})
endfunction(use_bundled_jemalloc)
