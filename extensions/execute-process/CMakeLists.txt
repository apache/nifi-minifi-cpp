# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (WIN32)
    return()
endif()

if (NOT (ENABLE_ALL OR ENABLE_EXECUTE_PROCESS))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-execute-process SHARED ${SOURCES})

target_link_libraries(minifi-execute-process ${LIBMINIFI})

register_extension(minifi-execute-process "EXECUTE PROCESS EXTENSION" EXECUTE-PROCESS-EXTENSION "This enables execute process support" "extensions/execute-process/test")

