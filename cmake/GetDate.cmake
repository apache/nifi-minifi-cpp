# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

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
