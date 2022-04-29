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

FetchContent_Declare(magic_enum
        URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.3.tar.gz
        URL_HASH SHA256=3cadd6a05f1bffc5141e5e731c46b2b73c2dbff025e723c8abaa659e0a24f072)

FetchContent_GetProperties(magic_enum)
if(NOT magic_enum_POPULATED)
    FetchContent_Populate(magic_enum)
    add_library(magic_enum INTERFACE)
    target_include_directories(magic_enum INTERFACE ${magic_enum_SOURCE_DIR}/include)
endif()
