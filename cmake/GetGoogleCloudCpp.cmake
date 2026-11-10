# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_GCP_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Google Cloud C++")
    find_package(GoogleCloudCpp REQUIRED)
    if (NOT SKIP_TESTS)
        find_package(GTest REQUIRED)
        include(GoogleTest)
        set_target_properties(GTest::gtest_main PROPERTIES IMPORTED_GLOBAL TRUE)
        set_target_properties(GTest::gmock PROPERTIES IMPORTED_GLOBAL TRUE)
        set(MINIFI_GCP_TEST_GTEST_LIBRARIES GTest::gtest_main GTest::gmock CACHE INTERNAL "")
    endif()
elseif(MINIFI_GCP_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Google Cloud C++ from source")
    include(GoogleCloudCpp)
endif()
