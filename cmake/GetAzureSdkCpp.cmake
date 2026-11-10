# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_AZURE_SDK_CPP_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Azure SDK for C++")
    find_package(AzureSDK REQUIRED)
elseif(MINIFI_AZURE_SDK_CPP_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Azure SDK for C++ from source")
    include(AzureSdkCpp)
endif()
