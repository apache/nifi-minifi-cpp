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

include(FetchContent)

set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(USE_THIRDPARTY_LIBRARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    prometheus-cpp
    URL "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.0.0.tar.gz"
    URL_HASH "SHA256=07018db604ea3e61f5078583e87c80932ea10c300d979061490ee1b7dc8e3a41"
    UPDATE_COMMAND git submodule update --init
)

FetchContent_MakeAvailable(prometheus-cpp)
