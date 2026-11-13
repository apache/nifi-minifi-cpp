# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Exclude conf for windows since we'll be doing the work in the WiX template

if (WIN32)
    return()
endif()


if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    set(MINIFI_INSTALLATION_TYPE_ENV_VALUE "FHS")
    set(MINIFI_HOME_ENV_VALUE "")
    set(MINIFI_EXEC_PATH "/usr/bin/minifi")
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    set(MINIFI_INSTALLATION_TYPE_ENV_VALUE "")
    set(MINIFI_HOME_ENV_VALUE "/opt/minifi-cpp")
    set(MINIFI_EXEC_PATH "/opt/minifi-cpp/bin/minifi")
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()

if(APPLE)
    configure_file(
            minifi.plist
            ${CMAKE_BINARY_DIR}/bin/minifi.plist
            @ONLY
    )
else()
    configure_file(
            minifi.service.in
            ${CMAKE_BINARY_DIR}/bin/minifi.service
            @ONLY
    )
endif()


configure_file(
        minifi.sh
        ${CMAKE_BINARY_DIR}/bin/minifi.sh
        @ONLY
)

if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.service DESTINATION /usr/lib/systemd/system COMPONENT bin)
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/minifi.sh DESTINATION bin COMPONENT bin)
    if (APPLE)
        install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.plist DESTINATION bin COMPONENT bin)
    else()
        install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.service DESTINATION bin COMPONENT bin)
    endif()
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()
