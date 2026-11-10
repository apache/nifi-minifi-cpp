# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_OPC_SOURCE STREQUAL "CONAN")
    message("Using Conan to install open62541")
    find_package(open62541 REQUIRED)
elseif(MINIFI_OPC_SOURCE STREQUAL "BUILD")
    message("Using CMake to build open62541 from source")
    include(Open62541)
endif()
