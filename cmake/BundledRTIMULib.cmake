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

function(use_bundled_rtimulib SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "RTIMULib.lib")
    else()
        set(BYPRODUCT "libRTIMULib.a")
    endif()

    # Build project
    ExternalProject_Add(
            rtimulib-external
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/RTIMULib/RTIMULib"
            BINARY_DIR "${BINARY_DIR}/thirdparty/RTIMULib/RTIMULib"
            CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/RTIMULib/RTIMULib/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(RTIMULib_FOUND "YES" CACHE STRING "" FORCE)
    set(RTIMULib_INCLUDE_DIRS "${SOURCE_DIR}/thirdparty/RTIMULib/RTIMULib" CACHE STRING "" FORCE)
    set(RTIMULib_LIBRARY "${BINARY_DIR}/thirdparty/RTIMULib/RTIMULib/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(RTIMULib_LIBRARIES ${RTIMULib_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(RTIMULib::RTIMULib STATIC IMPORTED)
    set_target_properties(RTIMULib::RTIMULib PROPERTIES IMPORTED_LOCATION "${RTIMULib_LIBRARY}")
    add_dependencies(RTIMULib::RTIMULib rtimulib-external)
    set_property(TARGET RTIMULib::RTIMULib APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${RTIMULib_INCLUDE_DIRS})
    set_property(TARGET RTIMULib::RTIMULib APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
endfunction(use_bundled_rtimulib)
