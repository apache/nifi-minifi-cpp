# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_SYSTEMD) OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

add_minifi_library(minifi-systemd SHARED "ConsumeJournald.cpp" "libwrapper/LibWrapper.cpp" "libwrapper/DlopenWrapper.cpp")

target_link_libraries(minifi-systemd ${LIBMINIFI} Threads::Threads)

register_extension(minifi-systemd "SYSTEMD EXTENSIONS" SYSTEMD-EXTENSIONS "Enabled log collection from journald" "extensions/systemd/tests")
