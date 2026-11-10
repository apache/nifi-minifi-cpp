# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_LUA_SCRIPTING OR ENABLE_PYTHON_SCRIPTING))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-script-extension SHARED ${SOURCES})

target_link_libraries(minifi-script-extension PRIVATE ${LIBMINIFI} Threads::Threads)
target_include_directories(minifi-script-extension PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")

register_extension(minifi-script-extension "SCRIPTING EXTENSIONS" SCRIPTING-EXTENSIONS "This enables scripting" "extensions/script/tests")
