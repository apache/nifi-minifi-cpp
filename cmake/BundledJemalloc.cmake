# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

function(use_bundled_jemalloc SOURCE_DIR BINARY_DIR)
    message("Using bundled jemalloc")

    # Define byproducts
    set(BYPRODUCT "lib/libjemalloc.a")

    # Build project
    ExternalProject_Add(
            jemalloc-external
            URL https://github.com/jemalloc/jemalloc/archive/refs/tags/5.3.1.tar.gz
            URL_HASH "SHA256=7b30f6116f11d736badd48c903cba2b3344a88d62e3a7b892434f870e860b1c0"
            PREFIX "${BINARY_DIR}/thirdparty/jemalloc"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${BINARY_DIR}/thirdparty/jemalloc-src"
            BUILD_COMMAND make build_lib_static
            CMAKE_COMMAND ""
            UPDATE_COMMAND ""
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/jemalloc-src/${BYPRODUCT}"
            INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Skipping install step."
            CONFIGURE_COMMAND ""
            PATCH_COMMAND ./autogen.sh && ./configure "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS}" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS}"
            STEP_TARGETS build
            EXCLUDE_FROM_ALL TRUE
            DOWNLOAD_NO_PROGRESS TRUE
            TLS_VERIFY TRUE
    )

    # Set variables
    set(JEMALLOC_FOUND "YES" CACHE STRING "" FORCE)
    set(JEMALLOC_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/jemalloc-src/include" CACHE STRING "" FORCE)
    set(JEMALLOC_LIBRARY "${BINARY_DIR}/thirdparty/jemalloc-src/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(JEMALLOC_LIBRARIES "${JEMALLOC_LIBRARY}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(jemalloc::jemalloc STATIC IMPORTED)
    set_target_properties(jemalloc::jemalloc PROPERTIES IMPORTED_LOCATION "${JEMALLOC_LIBRARY}")
    add_dependencies(jemalloc::jemalloc jemalloc-external)
    file(MAKE_DIRECTORY ${JEMALLOC_INCLUDE_DIRS})
    set_property(TARGET jemalloc::jemalloc APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${JEMALLOC_INCLUDE_DIRS})
endfunction(use_bundled_jemalloc)
