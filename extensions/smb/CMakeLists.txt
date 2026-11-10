# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (WIN32 AND ENABLE_SMB))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-smb SHARED ${SOURCES})
target_link_libraries(minifi-smb ${LIBMINIFI} Mpr)
target_include_directories(minifi-smb PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")

register_extension(minifi-smb "SMB EXTENSIONS" SMB-EXTENSIONS "This enables SMB support" "extensions/smb/tests")

