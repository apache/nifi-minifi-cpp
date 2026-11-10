# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (SKIP_TESTS AND NOT ENABLE_TEST_PROCESSORS)
    return()
endif()

include(../ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-test-processors SHARED ${SOURCES})

target_link_libraries(minifi-test-processors ${LIBMINIFI})

register_extension(minifi-test-processors "TEST-PROCESSORS EXTENSION" TEST-PROCESSORS-EXTENSION "This enables processors purely for testing purposes" "" "CI_ONLY_INSTALL")

