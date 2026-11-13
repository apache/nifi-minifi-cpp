# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_SQL))
    return()
endif()

if(WIN32)
    find_package(ODBC REQUIRED)
else()
    include(GetIODBC)
endif()

include(GetSOCI)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(SYSTEM ../../thirdparty/rapidjson-1.1.0/include/ ../../thirdparty/rapidjson-1.1.0/include/rapidjson)
include_directories(".")

file(GLOB SOURCES  "*.cpp" "services/*.cpp" "processors/*.cpp"  "data/*.cpp")

add_minifi_library(minifi-sql SHARED ${SOURCES})

target_link_libraries(minifi-sql SOCI::SOCI)

target_link_libraries(minifi-sql ${LIBMINIFI} Threads::Threads)

register_extension(minifi-sql "SQL EXTENSIONS" SQL-EXTENSIONS "Enables the SQL Suite of Tools" "extensions/sql/tests")
