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
option(SYSTEM_SIGSLOT "Use the system version of the palacaze/sigslot library. If turned off, it's built during minifi build." OFF)
if(SYSTEM_SIGSLOT)
    find_package(PalSigSlot 1.2)
else()
    FetchContent_Declare(sigslot_src
        URL      https://github.com/palacaze/sigslot/archive/refs/tags/v1.2.1.tar.gz
        URL_HASH SHA256=180b45e41676a730220e3a9af6ee71b761f23b8f6ade73c8f2aa20b677504934
    )
    FetchContent_MakeAvailable(sigslot_src)
endif()
