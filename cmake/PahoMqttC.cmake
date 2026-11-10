# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

# Set build options
set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(PAHO_WITH_SSL ON CACHE BOOL "" FORCE)
set(PAHO_HIGH_PERFORMANCE ON CACHE BOOL "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/paho-mqtt/all/patches/cmake-openssl.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\")")

FetchContent_Declare(
        paho.mqtt.c-external
        URL "https://github.com/eclipse-paho/paho.mqtt.c/archive/refs/tags/v1.3.16.tar.gz"
        URL_HASH "SHA256=8b960f51edc7e03507637d987882bc486d8f4be6e79431bf99e2763344fd14c5"
        PATCH_COMMAND "${PC}"
        SYSTEM
)

FetchContent_MakeAvailable(paho.mqtt.c-external)

target_link_libraries(common_ssl_obj_static PUBLIC OpenSSL::SSL OpenSSL::Crypto)
