# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

cmake_minimum_required(VERSION 3.15)
project(rpmalloc C)

# rpmalloc does not ship a CMake build; this mirrors the bundled RpMalloc.cmake target.
add_library(rpmalloc STATIC rpmalloc/rpmalloc.c)
target_include_directories(rpmalloc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rpmalloc>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(rpmalloc PRIVATE ENABLE_OVERRIDE=1 ENABLE_PRELOAD=1)

include(GNUInstallDirs)
install(TARGETS rpmalloc ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES rpmalloc/rpmalloc.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
