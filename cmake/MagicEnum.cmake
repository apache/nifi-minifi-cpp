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
        URL https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.8.tar.gz
        URL_HASH SHA256=1e54959a3f3cb675938d858603ad69d0f3f7c82439fc2bf86d7232daec2bd10e
        SYSTEM)

FetchContent_MakeAvailable(magic_enum)
