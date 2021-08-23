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

# Set build options
set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

if (OPENSSL_OFF)
    set(PAHO_WITH_SSL OFF CACHE BOOL "" FORCE)
else()
    set(PAHO_WITH_SSL ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    paho.mqtt.c-external
    URL "https://github.com/eclipse/paho.mqtt.c/archive/refs/tags/v1.3.9.tar.gz"
    URL_HASH "SHA256=386c9b5fa1cf6d0d516db12d57fd8f6a410dd0fdc5e9a2da870aae437a2535ed"
)

FetchContent_MakeAvailable(paho.mqtt.c-external)

# Set dependencies and target to link to
if (NOT OPENSSL_OFF)
    add_library(paho.mqtt.c ALIAS paho-mqtt3cs-static)
else()
    add_library(paho.mqtt.c ALIAS paho-mqtt3c-static)
endif()
