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

if (NOT (ENABLE_ALL OR ENABLE_KAFKA))
    return()
endif ()

include(Fetchlibrdkafka)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-kafka SHARED ${SOURCES})

target_link_libraries(minifi-kafka minifi-cpp-extension-lib Threads::Threads)

get_target_property(RDKAFKA_INCLUDE_DIRS rdkafka INCLUDE_DIRECTORIES)
target_include_directories(minifi-kafka SYSTEM PUBLIC ${RDKAFKA_INCLUDE_DIRS})
target_link_libraries(minifi-kafka rdkafka)

register_c_api_extension(minifi-kafka "RDKAFKA EXTENSIONS" RDKAFKA-EXTENSIONS "This Enables librdkafka functionality including PublishKafka" "extensions/kafka/tests")

