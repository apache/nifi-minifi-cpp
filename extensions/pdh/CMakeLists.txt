# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (WIN32 AND (ENABLE_ALL OR ENABLE_PDH)))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-pdh SHARED ${SOURCES})

target_link_libraries(minifi-pdh ${LIBMINIFI} pdh)

register_extension(minifi-pdh "PDH EXTENSIONS" PDH-EXTENSIONS "This enables PDH support" "extensions/pdh/tests")

