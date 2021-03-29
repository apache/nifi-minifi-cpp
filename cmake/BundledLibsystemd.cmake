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

function(use_bundled_libsystemd SOURCE_DIR BINARY_DIR)
    find_program(MESON meson)
    find_program(NINJA ninja)

    if(NOT MESON OR NOT NINJA)
        message(FATAL_ERROR "meson and ninja are required for libsystemd build")
    endif()

    set(LIBSYSTEMD_BIN_DIR "${BINARY_DIR}/thirdparty/libsystemd-install" CACHE STRING "" FORCE)
    set(LIBSYSTEMD_SRC_DIR "${BINARY_DIR}/thirdparty/libsystemd-src" CACHE STRING "" FORCE)
    set(BYPRODUCT libsystemd.a)

    ExternalProject_Add(libsystemd-external
        GIT_REPOSITORY https://github.com/systemd/systemd-stable.git
        GIT_TAG v247-stable
        SOURCE_DIR "${LIBSYSTEMD_SRC_DIR}"
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND "${MESON}" --buildtype=$<IF:$<CONFIG:Debug>,debug,release> -Dstatic-libsystemd=pic <SOURCE_DIR> "${LIBSYSTEMD_BIN_DIR}"
        BUILD_COMMAND "${NINJA}" -v -C "${LIBSYSTEMD_BIN_DIR}" version.h && "${NINJA}" -v -C "${LIBSYSTEMD_BIN_DIR}" libsystemd.a
        INSTALL_COMMAND mkdir -p "${LIBSYSTEMD_BIN_DIR}/include/systemd" && cp -rv "${LIBSYSTEMD_SRC_DIR}/src/systemd" "${LIBSYSTEMD_BIN_DIR}/include" && rm -v "${LIBSYSTEMD_BIN_DIR}/include/systemd/meson.build"
        BUILD_BYPRODUCTS "${LIBSYSTEMD_BIN_DIR}/${BYPRODUCT}"
        EXCLUDE_FROM_ALL=TRUE
    )

    SET(LIBSYSTEMD_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBSYSTEMD_INCLUDE_DIRS "${LIBSYSTEMD_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(LIBSYSTEMD_LIBRARIES "${LIBSYSTEMD_BIN_DIR}/${BYPRODUCT}" CACHE STRING "" FORCE)

    file(MAKE_DIRECTORY ${LIBSYSTEMD_INCLUDE_DIRS})

    add_library(libsystemd STATIC IMPORTED)
    set_target_properties(libsystemd PROPERTIES IMPORTED_LOCATION "${LIBSYSTEMD_LIBRARIES}")
    add_dependencies(libsystemd libsystemd-external)
    target_include_directories(libsystemd INTERFACE "${LIBSYSTEMD_INCLUDE_DIRS}")
endfunction(use_bundled_libsystemd)
