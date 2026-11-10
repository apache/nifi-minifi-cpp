# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_GCP))
    return()
endif()

include(GetGoogleCloudCpp)
include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
file(GLOB SOURCES "*.cpp" "controllerservices/*.cpp" "processors/*.cpp")

add_minifi_library(minifi-gcp SHARED ${SOURCES})

if (NOT WIN32)
    target_compile_options(minifi-gcp PRIVATE -Wno-error=deprecated-declarations)  # Suppress deprecation warnings for std::rel_ops usage
endif()
target_link_libraries(minifi-gcp minifi-cpp-extension-lib google-cloud-cpp::storage)

target_include_directories(minifi-gcp SYSTEM PUBLIC ${google-cloud-cpp_INCLUDE_DIRS})

register_c_api_extension(minifi-gcp "GCP EXTENSIONS" GCP-EXTENSIONS "This enables Google Cloud Platform support" "extensions/gcp/tests")
