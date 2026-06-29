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

set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    pugixml
    URL      https://github.com/zeux/pugixml/archive/refs/tags/v1.16.tar.gz
    URL_HASH SHA256=357bcab8877dc9943f355d3a72daba1b053238ba955f50fa81586afb65090219
)
FetchContent_MakeAvailable(pugixml)
