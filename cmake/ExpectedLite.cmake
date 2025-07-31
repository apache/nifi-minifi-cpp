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

FetchContent_Declare(expected-lite
    URL      https://github.com/martinmoene/expected-lite/archive/refs/tags/v0.9.0.tar.gz
    URL_HASH SHA256=e1b3ac812295ef8512c015d8271204105a71957323f8ab4e75f6856d71b8868d
    SYSTEM
)

# Due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119714
add_compile_definitions(nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD)
FetchContent_MakeAvailable(expected-lite)
