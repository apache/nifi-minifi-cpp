# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_PROMETHEUS OR ENABLE_ALL))
    return()
endif()

include(GetPrometheus)
include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp")

add_minifi_library(minifi-prometheus SHARED ${SOURCES})

target_link_libraries(minifi-prometheus ${LIBMINIFI} ${_prometheus_targets})
target_include_directories(minifi-prometheus PUBLIC ${prometheus-cpp_INCLUDE_DIRS})

register_extension(minifi-prometheus "PROMETHEUS EXTENSIONS" PROMETHEUS-EXTENSIONS "This enables Prometheus support" "extensions/prometheus/tests")
