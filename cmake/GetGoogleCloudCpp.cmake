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
