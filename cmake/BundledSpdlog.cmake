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

function(use_bundled_spdlog SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            set(BYPRODUCT "lib/spdlogd.lib")
        else()
            set(BYPRODUCT "lib/spdlog.lib")
        endif()
    else()
        include(GNUInstallDirs)
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            set(BYPRODUCT "${CMAKE_INSTALL_LIBDIR}/libspdlogd.a")
        else()
            set(BYPRODUCT "${CMAKE_INSTALL_LIBDIR}/libspdlog.a")
        endif()
    endif()

    # Set build options
    set(SPDLOG_SOURCE_DIR "${BINARY_DIR}/thirdparty/spdlog-src")
    set(SPDLOG_INSTALL_DIR "${BINARY_DIR}/thirdparty/spdlog-install")
    set(SPDLOG_LIBRARY "${SPDLOG_INSTALL_DIR}/${BYPRODUCT}")
    set(SPDLOG_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${SPDLOG_INSTALL_DIR}"
            "-DSPDLOG_BUILD_EXAMPLE=OFF"
            "-DSPDLOG_BUILD_TESTS=OFF"
            "-DSPDLOG_BUILD_TESTING=OFF"
            "-DSPDLOG_BUILD_BENCH=OFF"
            "-DSPDLOG_BUILD_SHARED=OFF")

    # Build project
    ExternalProject_Add(
            spdlog-external
            URL "https://github.com/gabime/spdlog/archive/v1.8.0.zip"
            SOURCE_DIR "${SPDLOG_SOURCE_DIR}"
            CMAKE_ARGS ${SPDLOG_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${SPDLOG_LIBRARY}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(SPDLOG_FOUND "YES" CACHE STRING "" FORCE)
    set(SPDLOG_INCLUDE_DIR "${SPDLOG_INSTALL_DIR}/include" CACHE STRING "" FORCE)
    set(SPDLOG_LIBRARY "${SPDLOG_INSTALL_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(SPDLOG_LIBRARIES ${SPDLOG_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(spdlog STATIC IMPORTED)
    add_dependencies(spdlog spdlog-external)
    file(MAKE_DIRECTORY ${SPDLOG_INCLUDE_DIR})
    set_target_properties(spdlog PROPERTIES
            IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}")

    if (NOT WIN32)
        set_property(TARGET spdlog APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_ENABLE_SYSLOG")
    endif()
endfunction(use_bundled_spdlog)
