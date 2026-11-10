# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
include(FetchContent)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/main_args.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
    cpplint
    URL           https://github.com/cpplint/cpplint/archive/refs/tags/2.0.2.tar.gz
    URL_HASH      SHA256=fc6d0cd40f934b58e8e0bb5eb5f1f2b651880b5fbc0e93a54e6fb6503f733b3d
    PATCH_COMMAND ${PC}
    SOURCE_DIR    ${CMAKE_BINARY_DIR}/cpplint
    SOURCE_SUBDIR does-not-exist   # only download the source
)
FetchContent_MakeAvailable(cpplint)

set(CPPLINT_DIR ${CMAKE_BINARY_DIR}/cpplint)
file(COPY "${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.py" DESTINATION "${CPPLINT_DIR}")
