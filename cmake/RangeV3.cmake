# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/ranges-v3/remove-deprecated.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(range-v3_src
    URL      https://github.com/ericniebler/range-v3/archive/refs/tags/0.12.0.tar.gz
    URL_HASH SHA256=015adb2300a98edfceaf0725beec3337f542af4915cec4d0b89fa0886f4ba9cb
    PATCH_COMMAND "${PC}"
    SYSTEM
)
FetchContent_MakeAvailable(range-v3_src)
target_compile_definitions(range-v3 INTERFACE RANGES_CXX_THREAD_LOCAL=201103L)
