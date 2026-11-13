# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT ENABLE_TEST_PROCESSORS)
    return()
endif ()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-stable-api-testing SHARED ${SOURCES})

target_link_libraries(minifi-stable-api-testing minifi-cpp-extension-lib)

register_c_api_extension(minifi-stable-api-testing "Stable API Sandbox" SANDBOX-EXTENSION "Stable API Sandbox" "extensions/stable-api-testing/tests")
