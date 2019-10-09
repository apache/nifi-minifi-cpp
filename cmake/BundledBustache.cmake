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

function(use_bundled_bustache SOURCE_DIR BINARY_DIR)
    # Find Boost
    find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/bustache.lib")
    else()
        set(BYPRODUCT "lib/libbustache.a")
    endif()

    # Set build options
    set(BUSTACHE_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/bustache-install")

    set(BUSTACHE_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BUSTACHE_BYPRODUCT_DIR}"
            "-DBUSTACHE_ENABLE_TESTING=OFF")

    # Build project
    ExternalProject_Add(
            bustache-external
            GIT "https://github.com/jamboree/bustache.git"
            GIT_TAG "42dee8ef9bbcae7e9a33500a116cfd9c314662d6"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/bustache-src"
            CMAKE_ARGS ${BUSTACHE_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BUSTACHE_BYPRODUCT_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(BUSTACHE_FOUND "YES" CACHE STRING "" FORCE)
    set(BUSTACHE_INCLUDE_DIR "${BUSTACHE_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
    set(BUSTACHE_LIBRARY "${BUSTACHE_BYPRODUCT_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(BUSTACHE::libbustache STATIC IMPORTED)
    set_target_properties(BUSTACHE::libbustache PROPERTIES IMPORTED_LOCATION "${BUSTACHE_LIBRARY}")
    add_dependencies(BUSTACHE::libbustache bustache-external)
    file(MAKE_DIRECTORY ${BUSTACHE_INCLUDE_DIR})
    set_property(TARGET BUSTACHE::libbustache APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${BUSTACHE_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})
    set_property(TARGET BUSTACHE::libbustache APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})
endfunction(use_bundled_bustache)
