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

set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(jsoncons
    URL      https://github.com/danielaparker/jsoncons/archive/refs/tags/v1.7.0.tar.gz
    URL_HASH SHA256=5a2aad4e791a1c93b0b0b326973459753a96a4c48d06d3035cd0ea0d262198d4
    SYSTEM
)

FetchContent_MakeAvailable(jsoncons)
