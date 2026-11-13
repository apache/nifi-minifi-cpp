# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_KUBERNETES_CLIENT_C_SOURCE STREQUAL "CONAN")
    message("Using Conan to install Kubernetes Client C")
    find_package(kubernetes REQUIRED)
elseif(MINIFI_KUBERNETES_CLIENT_C_SOURCE STREQUAL "BUILD")
    message("Using CMake to build Kubernetes Client C from source")
    include(KubernetesClientC)
endif()
