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

function(use_bundled_libyamlc SOURCE_DIR BINARY_DIR)
    set(YAMLC_SOURCE_DIR "${BINARY_DIR}/thirdparty/libyaml-src")
    set(YAMLC_BINARY_DIR "${BINARY_DIR}/thirdparty/libyaml-build")

    # Define byproducts
    if (WIN32)
      set(LIB_YAML "${YAMLC_BINARY_DIR}/${CMAKE_BUILD_TYPE}/yamlc.lib" CACHE STRING "" FORCE)
    else()
      set(LIB_YAML "${YAMLC_BINARY_DIR}/libyamlc.a" CACHE STRING "" FORCE)
    endif()

    set(YAMLC_BYPRODUCT "${LIB_YAML}")
    set(LIBYAMLC_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
      "-DYAML_STATIC_LIB_NAME=yamlc")

    # Build project
    ExternalProject_Add(
      libyamlc-external
      GIT_REPOSITORY "https://github.com/yaml/libyaml.git"
      GIT_TAG "release/0.2.3"
      SOURCE_DIR "${YAMLC_SOURCE_DIR}"
      BINARY_DIR "${YAMLC_BINARY_DIR}"
      CMAKE_ARGS "${LIBYAMLC_CMAKE_ARGS}"
      INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "skip install step"
      BUILD_BYPRODUCTS "${YAMLC_BYPRODUCT}"
      EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(YAMLC_INCLUDE_DIR "${YAMLC_SOURCE_DIR}/include" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(yaml STATIC IMPORTED)
    add_dependencies(yaml libyamlc-external)    
    add_library(libyaml-c INTERFACE)

    set_property(TARGET yaml PROPERTY IMPORTED_LOCATION "${YAMLC_BYPRODUCT}")

    target_include_directories(libyaml-c INTERFACE "${YAMLC_INCLUDE_DIR}")
    target_compile_definitions(libyaml-c INTERFACE "YAML_DECLARE_STATIC")
    target_link_libraries(libyaml-c INTERFACE yaml)
endfunction(use_bundled_libyamlc)
