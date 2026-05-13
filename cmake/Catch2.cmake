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

FetchContent_Declare(
        Catch2
        URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.0.tar.gz
        URL_HASH SHA256=9650c55e497759cc39b977e45524bc8acb15256061c112080916ab6cb0b1ea66
        SYSTEM
)
FetchContent_MakeAvailable(Catch2)
