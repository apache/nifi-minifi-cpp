# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
if (NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (ENABLE_ALL OR ENABLE_KUBERNETES)))
    return()
endif()

include(GetKubernetesClientC)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp" "controllerservice/*.cpp" "processors/*.cpp")
add_minifi_library(minifi-kubernetes-extensions SHARED ${SOURCES})
target_link_libraries(minifi-kubernetes-extensions ${LIBMINIFI} kubernetes::kubernetes)

register_extension(minifi-kubernetes-extensions "KUBERNETES EXTENSIONS" KUBERNETES-EXTENSIONS "This enables Kubernetes support" "extensions/kubernetes/tests")
