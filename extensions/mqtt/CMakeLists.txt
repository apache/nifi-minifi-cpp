# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if(NOT (ENABLE_ALL OR ENABLE_MQTT))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(./processors ../../libminifi/include  ../../libminifi/include/core)

file(GLOB SOURCES "processors/*.cpp")

add_minifi_library(minifi-mqtt-extensions SHARED ${SOURCES})

target_link_libraries(minifi-mqtt-extensions ${LIBMINIFI})

include(GetPahoMqttC)
target_link_libraries(minifi-mqtt-extensions eclipse-paho-mqtt-c::paho-mqtt3as-static)

register_extension(minifi-mqtt-extensions "MQTT EXTENSIONS" MQTT-EXTENSIONS "This Enables MQTT functionality including PublishMQTT/ConsumeMQTT" "${CMAKE_CURRENT_SOURCE_DIR}/tests")
