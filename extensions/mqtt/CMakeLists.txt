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

if(NOT (ENABLE_ALL OR ENABLE_MQTT))
    return()
endif()

include(Protobuf)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

set(SPARKPLUG_PROTOBUF_GENERATED_DIR ${CMAKE_BINARY_DIR}/sparkplug-protobuf-generated)
file(MAKE_DIRECTORY ${SPARKPLUG_PROTOBUF_GENERATED_DIR})

add_custom_command(
    OUTPUT  ${SPARKPLUG_PROTOBUF_GENERATED_DIR}/sparkplug_b.pb.h ${SPARKPLUG_PROTOBUF_GENERATED_DIR}/sparkplug_b.pb.cc
    COMMAND ${PROTOBUF_COMPILER} -I=${CMAKE_CURRENT_SOURCE_DIR}/protos/ -I=${protobuf_SOURCE_DIR}/src --cpp_out=${SPARKPLUG_PROTOBUF_GENERATED_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/protos/sparkplug_b.proto
    DEPENDS protobuf::protoc)

add_library(sparkplug-protos ${SPARKPLUG_PROTOBUF_GENERATED_DIR}/sparkplug_b.pb.cc)
target_include_directories(sparkplug-protos SYSTEM PRIVATE BEFORE "${SPARKPLUG_PROTOBUF_GENERATED_DIR}" "${PROTOBUF_INCLUDE_DIR}")
target_link_libraries(sparkplug-protos protobuf::libprotobuf)

if (NOT WIN32)
    set_source_files_properties(${SPARKPLUG_PROTOBUF_GENERATED_DIR}/sparkplug_b.pb.cc PROPERTIES COMPILE_FLAGS -Wno-error)
endif()

file(GLOB SOURCES "processors/*.cpp" "controllers/*.cpp")

add_minifi_library(minifi-mqtt-extensions SHARED ${SOURCES})

target_link_libraries(minifi-mqtt-extensions ${LIBMINIFI} sparkplug-protos)
target_include_directories(minifi-mqtt-extensions PRIVATE BEFORE "./processors" "./controllers" "../../libminifi/include"  "../../libminifi/include/core")
target_include_directories(minifi-mqtt-extensions SYSTEM PRIVATE BEFORE "${SPARKPLUG_PROTOBUF_GENERATED_DIR}" "${PROTOBUF_INCLUDE_DIR}")

include(PahoMqttC)
target_link_libraries(minifi-mqtt-extensions paho.mqtt.c)

register_extension(minifi-mqtt-extensions "MQTT EXTENSIONS" MQTT-EXTENSIONS "This Enables MQTT functionality including PublishMQTT/ConsumeMQTT" "${CMAKE_CURRENT_SOURCE_DIR}/tests")
register_extension_linter(minifi-mqtt-extensions-linter)
