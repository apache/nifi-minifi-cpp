# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_COUCHBASE))
    return()
endif()

include(GetCouchbase)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "controllerservices/*.cpp" "processors/*.cpp")

add_minifi_library(minifi-couchbase SHARED ${SOURCES})

target_include_directories(minifi-couchbase PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}" "controllerservices" "processors")
target_include_directories(minifi-couchbase SYSTEM PRIVATE ${COUCHBASE_INCLUDE_DIR})
# Demote preprocessor warnings so <ciso646> doesn't break the build
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(minifi-couchbase PRIVATE -Wno-error=cpp)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(minifi-couchbase PRIVATE -Wno-error=\#warnings)
endif()
target_link_libraries(minifi-couchbase ${LIBMINIFI} couchbase_cxx_client::couchbase_cxx_client hdr_histogram_static snappy llhttp::llhttp taocpp::json)

register_extension(minifi-couchbase "COUCHBASE EXTENSIONS" COUCHBASE-EXTENSIONS "This enables Couchbase support" "extensions/couchbase/tests")
