# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

file(GLOB SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")

add_library(mock-minifi STATIC ${SRC_FILES})
target_link_libraries(mock-minifi PUBLIC minifi-cpp-extension-lib minifi-core-framework-common minifi-c-api)
target_include_directories(mock-minifi PUBLIC include)
