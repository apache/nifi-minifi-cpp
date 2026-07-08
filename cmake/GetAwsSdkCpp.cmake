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

if(MINIFI_AWS_SDK_CPP_SOURCE STREQUAL "CONAN")
    message("Using Conan to install AWS SDK for C++")
    find_package(AWSSDK REQUIRED GLOBAL)
elseif(MINIFI_AWS_SDK_CPP_SOURCE STREQUAL "BUILD")
    message("Using CMake to build AWS SDK for C++ from source")
    include(BundledAwsSdkCpp)
    use_bundled_libaws(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()
