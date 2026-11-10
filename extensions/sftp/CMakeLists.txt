# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_SFTP))
    return()
endif()

include(GetLibSSH2)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(client processors)

file(GLOB SOURCES  "*.cpp" "client/*.cpp" "processors/*.cpp")

add_minifi_library(minifi-sftp SHARED ${SOURCES})
set_target_properties(minifi-sftp PROPERTIES HAS_CUSTOM_INITIALIZER TRUE)

target_link_libraries(minifi-sftp ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-sftp Libssh2::libssh2 rapidjson)

register_extension(minifi-sftp "SFTP EXTENSIONS" SFTP "This enables SFTP support" "extensions/sftp/tests")
