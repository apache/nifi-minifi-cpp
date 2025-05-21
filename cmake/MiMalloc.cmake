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
        mimalloc
        URL      https://github.com/microsoft/mimalloc/archive/refs/tags/v2.0.6.tar.gz
        URL_HASH SHA256=9f05c94cc2b017ed13698834ac2a3567b6339a8bde27640df5a1581d49d05ce5
        SYSTEM
)
FetchContent_MakeAvailable(mimalloc)
