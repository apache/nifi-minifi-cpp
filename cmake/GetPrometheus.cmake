# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_PROMETHEUS_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Prometheus")
    find_package(prometheus-cpp REQUIRED)
    set(_prometheus_targets prometheus-cpp::prometheus-cpp)
elseif(MINIFI_PROMETHEUS_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Prometheus from source")
    include(Prometheus)
    set(_prometheus_targets prometheus-cpp::core prometheus-cpp::pull)
endif()
