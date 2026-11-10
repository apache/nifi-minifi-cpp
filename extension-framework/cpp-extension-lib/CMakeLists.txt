# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

file(GLOB SOURCES src/core/*.cpp src/core/logging/*.cpp src/utils/*.cpp)

add_minifi_library(minifi-cpp-extension-lib STATIC ${SOURCES})
target_include_directories(minifi-cpp-extension-lib PUBLIC include)
target_link_libraries(minifi-cpp-extension-lib PUBLIC minifi-core-framework-common minifi-c-api)

add_subdirectory(libtest)
add_subdirectory(mocklib)
