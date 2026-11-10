# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_AWS_SDK_CPP_SOURCE STREQUAL "CONAN")
    message("Using Conan to install AWS SDK for C++")
    find_package(AWSSDK REQUIRED GLOBAL)
elseif(MINIFI_AWS_SDK_CPP_SOURCE STREQUAL "BUILD")
    message("Using CMake to build AWS SDK for C++ from source")
    include(BundledAwsSdkCpp)
    use_bundled_libaws(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
