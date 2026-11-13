# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (ENABLE_ALL OR ENABLE_PROCFS)))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp" "processors/*.cpp" "utils/*.cpp")

add_minifi_library(minifi-procfs SHARED ${SOURCES})
target_link_libraries(minifi-procfs ${LIBMINIFI})

register_extension(minifi-procfs "PROCFS EXTENSIONS" PROCFS-EXTENSIONS "Enabled metrics collection from procfs" "extensions/procfs/tests")

