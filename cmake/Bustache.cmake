# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)
include(GetFmt)

set(BUSTACHE_USE_FMT ON CACHE STRING "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/bustache/all/patches/add-append.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/bustache/all/patches/fix-deprecated-literal-operator.patch")
set(PATCH_FILE_3 "${CMAKE_SOURCE_DIR}/thirdparty/bustache/all/patches/remove_installs.patch")

set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\") &&\
            (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_3}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_3}\\\")")


FetchContent_Declare(Bustache
        GIT_REPOSITORY  https://github.com/jamboree/bustache.git
        GIT_TAG         af10816ce3d2ff8577fd27caa2c1005c01d8069b
        PATCH_COMMAND   "${PC}"
        SYSTEM
)
FetchContent_MakeAvailable(Bustache)
