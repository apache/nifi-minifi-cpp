# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_PAHO_MQTT_C_SOURCE STREQUAL "CONAN")
    message("Using Conan to install paho-mqtt-c")
    find_package(eclipse-paho-mqtt-c REQUIRED)
elseif(MINIFI_PAHO_MQTT_C_SOURCE STREQUAL "BUILD")
    message("Using CMake to build paho-mqtt-c from source")
    include(PahoMqttC)
endif()
