# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(OPEN62541_VERSION "v1.5.10" CACHE STRING "" FORCE)
set(UA_ENABLE_ENCRYPTION ON CACHE BOOL "" FORCE)
set(UA_ENABLE_ENCRYPTION_OPENSSL ON CACHE BOOL "" FORCE)
set(UA_FORCE_WERROR OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_DEBUG_SANITIZER OFF CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/open62541/all/patches/open62541.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    open62541
    URL "https://github.com/open62541/open62541/archive/refs/tags/${OPEN62541_VERSION}.tar.gz"
    URL_HASH "SHA256=b05e47734e2e39c0b00511ed6dc22e71f1b5bf7864b86280653a5128d171c0ed"
    PATCH_COMMAND "${PC}"
    EXCLUDE_FROM_ALL
    DOWNLOAD_NO_PROGRESS TRUE
    TLS_VERIFY TRUE
    SYSTEM
)

FetchContent_MakeAvailable(open62541)

add_dependencies(open62541 OpenSSL::Crypto OpenSSL::SSL)

if(TARGET openssl-external)
    add_dependencies(open62541-object openssl-external)
    add_dependencies(open62541-plugins openssl-external)
endif()
