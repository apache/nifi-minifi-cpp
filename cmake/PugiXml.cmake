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
    URL      https://github.com/zeux/pugixml/archive/refs/tags/v1.15.tar.gz
    URL_HASH SHA256=b39647064d9e28297a34278bfb897092bf33b7c487906ddfc094c9e8868bddcb
)
FetchContent_MakeAvailable(pugixml)
