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

function(use_bundled_yamlcpp SOURCE_DIR BINARY_DIR)
    if (WIN32)
        set(LIBDIR "lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
    endif()

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "${LIBDIR}/yaml-cpp.lib")
    else()
        set(BYPRODUCT "${LIBDIR}/libyaml-cpp.a")
    endif()

    # Set build options
    set(YAMLCPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/yaml-cpp-install"
            "-DCMAKE_DEBUG_POSTFIX="
            "-DBUILD_SHARED_LIBS=OFF"
            "-DYAML_CPP_BUILD_TESTS=OFF"
            "-DYAML_CPP_BUILD_TOOLS=OFF")

    # Build project
    ExternalProject_Add(
            yaml-cpp-external
            URL "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz"
            URL_HASH "SHA256=43e6a9fcb146ad871515f0d0873947e5d497a1c9c60c58cb102a97b47208b7c3"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/yaml-cpp-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/yaml-cpp-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${YAMLCPP_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/yaml-cpp-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(YAMLCPP_FOUND "YES" CACHE STRING "" FORCE)
    set(YAMLCPP_INCLUDE_DIR "${BINARY_DIR}/thirdparty/yaml-cpp-install/include" CACHE STRING "" FORCE)
    set(YAMLCPP_LIBRARY "${BINARY_DIR}/thirdparty/yaml-cpp-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(YAMLCPP_LIBRARIES ${YAMLCPP_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${YAMLCPP_INCLUDE_DIR})

    add_library(yaml-cpp STATIC IMPORTED)
    set_target_properties(yaml-cpp PROPERTIES IMPORTED_LOCATION "${YAMLCPP_LIBRARY}")
    add_dependencies(yaml-cpp yaml-cpp-external)
    target_include_directories(yaml-cpp INTERFACE ${YAMLCPP_INCLUDE_DIR})
endfunction(use_bundled_yamlcpp)
