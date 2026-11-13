# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_SPLUNK))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-splunk SHARED ${SOURCES})

target_link_libraries(minifi-splunk ${LIBMINIFI})

register_extension(minifi-splunk "SPLUNK EXTENSIONS" SPLUNK-EXTENSIONS "This enables Splunk support" "extensions/splunk/tests")

