# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (WIN32 AND (ENABLE_ALL OR ENABLE_WEL)))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "wel/*.cpp")

add_minifi_library(minifi-wel SHARED ${SOURCES})

target_link_libraries(minifi-wel ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-wel pugixml::pugixml ZLIB::ZLIB Wevtapi.lib)

register_extension(minifi-wel "WEL EXTENSIONS" WEL-EXTENSION "Enables the suite of Windows Event Log extensions." "extensions/windows-event-log/tests")
