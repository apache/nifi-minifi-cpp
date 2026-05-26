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

include(FetchContent)

if (WIN32)
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

    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cldr-common-38.1/common/supplemental/windowsZones.xml
        DESTINATION ${CMAKE_BINARY_DIR}/tzdata)

    file(COPY ${tzdata_SOURCE_DIR}/
        DESTINATION ${CMAKE_BINARY_DIR}/tzdata)

    install(DIRECTORY ${tzdata_SOURCE_DIR}/
        DESTINATION tzdata
        COMPONENT bin)

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cldr-common-38.1/common/supplemental/windowsZones.xml
        DESTINATION tzdata
        COMPONENT bin)
endif()

FetchContent_Declare(date_src
    URL         https://github.com/HowardHinnant/date/archive/refs/tags/v3.0.4.tar.gz
    URL_HASH    SHA256=56e05531ee8994124eeb498d0e6a5e1c3b9d4fccbecdf555fe266631368fb55f
    SYSTEM
)
FetchContent_GetProperties(date_src)
if (NOT date_src_POPULATED)
    FetchContent_Populate(date_src)
    set(DATE_INCLUDE_DIR "${date_src_SOURCE_DIR}/include" CACHE STRING "" FORCE)
    add_library(date INTERFACE)
    add_library(date::date ALIAS date)
    target_sources(date INTERFACE ${DATE_INCLUDE_DIR}/date/date.h)
    target_include_directories(date SYSTEM INTERFACE ${DATE_INCLUDE_DIR})
    target_compile_features(date INTERFACE cxx_std_11)

    add_library(date-tz STATIC ${date_src_SOURCE_DIR}/src/tz.cpp)
    add_library(date::tz ALIAS date-tz)
    target_include_directories(date-tz SYSTEM PUBLIC ${DATE_INCLUDE_DIR})
    target_compile_features(date-tz PUBLIC cxx_std_11)
    target_compile_options(date-tz PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)
    target_compile_definitions(date-tz PRIVATE AUTO_DOWNLOAD=0 HAS_REMOTE_API=0)
    if (WIN32)
        target_compile_definitions(date-tz PRIVATE INSTALL=. PUBLIC USE_OS_TZDB=0)
    else()
        target_compile_definitions(date-tz PUBLIC USE_OS_TZDB=1)
    endif()
    if (NOT MSVC)
        find_package(Threads)
        target_link_libraries(date-tz PUBLIC Threads::Threads)
    endif()
endif()
