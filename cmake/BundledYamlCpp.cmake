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
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/libyaml-cppmd.lib")
    else()
        set(BYPRODUCT "lib/libyaml-cpp.a")
    endif()

    # Set build options
    set(YAMLCPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/yaml-cpp-install"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

    # Build project
    ExternalProject_Add(
            yaml-cpp-external
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/yaml-cpp-yaml-cpp-20171024"
            CMAKE_ARGS ${YAMLCPP_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/yaml-cpp-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(YAMLCPP_FOUND "YES" CACHE STRING "" FORCE)
    set(YAMLCPP_INCLUDE_DIR "${SOURCE_DIR}/thirdparty/yaml-cpp-yaml-cpp-20171024/include" CACHE STRING "" FORCE)
    set(YAMLCPP_LIBRARY "${BINARY_DIR}/thirdparty/yaml-cpp-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(YAMLCPP_LIBRARIES ${YAMLCPP_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(yaml-cpp STATIC IMPORTED)
    set_target_properties(yaml-cpp PROPERTIES IMPORTED_LOCATION "${YAMLCPP_LIBRARY}")
    add_dependencies(yaml-cpp yaml-cpp-external)
    set_property(TARGET yaml-cpp APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${YAMLCPP_INCLUDE_DIR})
endfunction(use_bundled_yamlcpp)
