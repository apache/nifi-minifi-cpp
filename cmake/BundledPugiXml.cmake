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

function(use_bundled_pugixml SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/pugixml.lib")
    else()
        set(BYPRODUCT "lib/libpugixml.a")
    endif()

    # Set build options
    set(PUGI_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/pugixml-install")

    set(PUGI_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${PUGI_BYPRODUCT_DIR}"
            "-DBUILD_TESTS=OFF"
            "-DBUILD_SHARED_AND_STATIC_LIBS=OFF"
            "-DBUILD_SHARED_LIBS=OFF")

    # Build project
    ExternalProject_Add(
            pugixml-external
            URL "https://github.com/zeux/pugixml/releases/download/v1.9/pugixml-1.9.tar.gz"
            URL_HASH "SHA256=d156d35b83f680e40fd6412c4455fdd03544339779134617b9b28d19e11fdba6"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/pugixml-src"
            CMAKE_ARGS ${PUGI_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${PUGI_BYPRODUCT_DIR}/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(PUGIXML_FOUND "YES" CACHE STRING "" FORCE)
    set(PUGIXML_INCLUDE_DIR "${PUGI_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
    set(PUGIXML_LIBRARY "${PUGI_BYPRODUCT_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(PUGI::libpugixml STATIC IMPORTED)
    set_target_properties(PUGI::libpugixml PROPERTIES IMPORTED_LOCATION "${PUGIXML_LIBRARY}")
    add_dependencies(PUGI::libpugixml pugixml-external)
    file(MAKE_DIRECTORY ${PUGIXML_INCLUDE_DIR})
    set_property(TARGET PUGI::libpugixml APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${PUGIXML_INCLUDE_DIR})
endfunction(use_bundled_pugixml)
