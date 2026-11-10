# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements



include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "processors/*.cpp" "controllers/*.cpp" "utils/*.cpp" "modbus/*.cpp")

add_minifi_library(minifi-standard-processors SHARED ${SOURCES})
target_include_directories(minifi-standard-processors PUBLIC "${CMAKE_SOURCE_DIR}/extensions/standard-processors")

include(GetJsoncons)
target_link_libraries(minifi-standard-processors ${LIBMINIFI} Threads::Threads range-v3::range-v3 asio::asio pugixml::pugixml jsoncons::jsoncons)


register_extension(minifi-standard-processors "STANDARD PROCESSORS" STANDARD-PROCESSORS "Provides standard processors" "extensions/standard-processors/tests/")

