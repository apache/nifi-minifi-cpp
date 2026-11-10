# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

add_library(libminifi-c-unittest INTERFACE)
target_link_libraries(libminifi-c-unittest INTERFACE minifi-cpp-extension-lib core-minifi)
target_include_directories(libminifi-c-unittest INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
