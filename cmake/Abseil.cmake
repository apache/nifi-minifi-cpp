#
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
#
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL absl-propagate-cxx)
set(ABSL_ENABLE_INSTALL ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE STRING "" FORCE)
FetchContent_Declare(
        absl
        URL      https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.tar.gz
        URL_HASH SHA256=dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4
)
FetchContent_MakeAvailable(absl)
