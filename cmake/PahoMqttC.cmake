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

FetchContent_Declare(
    paho.mqtt.c-external
    GIT_REPOSITORY "https://github.com/eclipse/paho.mqtt.c.git"
    GIT_TAG "3b7ae6348bc917d42c04efa962e4868c09bbde9f" # Once tagged as v1.3.9 release, but tags may move
)

FetchContent_GetProperties(paho.mqtt.c-external)
if(NOT paho.mqtt.c-external_POPULATED)
    FetchContent_Populate(paho.mqtt.c-external)

    # Set build options
    option(PAHO_BUILD_STATIC "" TRUE)
    option(PAHO_BUILD_SHARED "" FALSE)
    option(PAHO_ENABLE_TESTING "" FALSE)

    if (OPENSSL_OFF)
        option(PAHO_WITH_SSL "" FALSE)
    else()
        option(PAHO_WITH_SSL "" TRUE)
    endif()

    add_subdirectory(${paho.mqtt.c-external_SOURCE_DIR} ${paho.mqtt.c-external_BINARY_DIR})
endif()

# Set dependencies and target to link to
if (NOT OPENSSL_OFF)
    # workaround
    if(TARGET paho-mqtt3cs)
        # this is selected for the first configure run
        add_library(paho.mqtt.c ALIAS paho-mqtt3cs)
    else()
        # this is selected for further configure runs
        add_library(paho.mqtt.c ALIAS paho-mqtt3cs-static)
    endif()
else()
    # workaround
    if(TARGET paho-mqtt3c)
        # this is selected for the first configure run
        add_library(paho.mqtt.c ALIAS paho-mqtt3c)
    else()
        # this is selected for further configure runs
        add_library(paho.mqtt.c ALIAS paho-mqtt3c-static)
    endif()
endif()
