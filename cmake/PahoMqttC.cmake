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
set(PAHO_WITH_SSL ON CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/paho-mqtt/cmake-openssl.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    paho.mqtt.c-external
    URL "https://github.com/eclipse/paho.mqtt.c/archive/refs/tags/v1.3.14.tar.gz"
    URL_HASH "SHA256=7af7d906e60a696a80f1b7c2bd7d6eb164aaad908ff4c40c3332ac2006d07346"
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(paho.mqtt.c-external)

# Set dependencies and target to link to
add_library(paho.mqtt.c ALIAS paho-mqtt3as-static)
add_dependencies(common_ssl_obj_static OpenSSL::SSL OpenSSL::Crypto)
