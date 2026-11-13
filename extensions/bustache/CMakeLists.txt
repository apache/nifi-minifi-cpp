# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_BUSTACHE))
    return()
endif()

include(GetBustache)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-bustache-extensions SHARED ${SOURCES})

target_link_libraries(minifi-bustache-extensions ${LIBMINIFI})
target_link_libraries(minifi-bustache-extensions bustache)

register_extension(minifi-bustache-extensions "BUSTACHE EXTENSIONS" BUSTACHE-EXTENSIONS "This enables bustache functionality including ApplyTemplate." "extensions/bustache/tests")
