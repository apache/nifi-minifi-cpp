# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (WIN32)
    include(FetchContent)

    # tzdata and windowsZones.xml from unicode cldr-common are required to be installed for date-tz operation on Windows
    FetchContent_Declare(tzdata
        URL         https://data.iana.org/time-zones/releases/tzdata2026b.tar.gz
        URL_HASH    SHA256=114543d9f19a6bfeb5bca43686aea173d38755a3db1f2eec112647ae92c6f544
        SYSTEM
    )
    FetchContent_GetProperties(tzdata)
    if (NOT tzdata_POPULATED)
        FetchContent_Populate(tzdata)
    endif()

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tzdata)

    file(COPY ${CMAKE_SOURCE_DIR}/thirdparty/cldr-common-38.1/common/supplemental/windowsZones.xml
        DESTINATION ${CMAKE_BINARY_DIR}/tzdata)

    file(COPY ${tzdata_SOURCE_DIR}/
        DESTINATION ${CMAKE_BINARY_DIR}/tzdata)

    install(DIRECTORY ${tzdata_SOURCE_DIR}/
        DESTINATION tzdata
        COMPONENT bin)

    install(FILES ${CMAKE_SOURCE_DIR}/thirdparty/cldr-common-38.1/common/supplemental/windowsZones.xml
        DESTINATION tzdata
        COMPONENT bin)
endif()

if(MINIFI_DATE_SOURCE STREQUAL "CONAN")
    message("Using Conan to install date")
    find_package(date REQUIRED)
    if(NOT TARGET date::tz)
        add_library(date::tz ALIAS date::date-tz)
    endif()
elseif(MINIFI_DATE_SOURCE STREQUAL "BUILD")
    message("Using CMake to build date from source")
    include(Date)
endif()
