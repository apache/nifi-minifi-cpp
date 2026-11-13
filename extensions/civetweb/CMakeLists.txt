# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_CIVET))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "processors/*.cpp" "protocols/*.cpp")

add_minifi_library(minifi-civet-extensions SHARED ${SOURCES})
target_include_directories(minifi-civet-extensions BEFORE PUBLIC
                           ${CMAKE_SOURCE_DIR}/libminifi/include
                           ${CMAKE_SOURCE_DIR}/libminifi/include/core
                           ${CMAKE_SOURCE_DIR}/thirdparty/
                           ${CIVETWEB_INCLUDE_DIRS}/include
                           ./include)
target_link_libraries(minifi-civet-extensions ${LIBMINIFI} Threads::Threads civetweb::civetweb-cpp)

register_extension(minifi-civet-extensions CIVETWEB CIVETWEB "This enables ListenHTTP" "extensions/civetweb/tests")
