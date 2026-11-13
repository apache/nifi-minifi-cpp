# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (NOT (ENABLE_ALL OR ENABLE_OPC))
    return()
endif()

include(GetOpen62541)

set( CMAKE_VERBOSE_MAKEFILE on )

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_minifi_library(minifi-opc-extensions SHARED ${SOURCES})

target_link_libraries(minifi-opc-extensions ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-opc-extensions ${CMAKE_DL_LIBS} spdlog::spdlog open62541::open62541)

register_extension(minifi-opc-extensions "OPC EXTENSIONS" OPC-EXTENSIONS "This enables OPC-UA support" "extensions/opc/tests")
