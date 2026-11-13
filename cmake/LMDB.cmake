# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/lmdb/all/patches/add-cmake-file.patch")
if (WIN32)
    set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/lmdb/all/patches/fix-windows-symbols.patch")
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p0 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p0 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")
else()
    set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p0 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p0 -N -i \\\"${PATCH_FILE_1}\\\")")
endif()

FetchContent_Declare(
        lmdb
        URL      https://github.com/LMDB/lmdb/archive/refs/tags/LMDB_1.0.2.tar.gz
        URL_HASH SHA256=17e4cb8e80874fccc1ff6f7d5fdb62f029624967d001ff40c47c0fb7a5b42d7d
        PATCH_COMMAND "${PC}"
        SOURCE_SUBDIR "libraries/liblmdb"
        SYSTEM
)

FetchContent_MakeAvailable(lmdb)

set(LMDB_INCLUDE_DIR "${lmdb_SOURCE_DIR}/libraries/liblmdb")

if (NOT TARGET lmdb::lmdb)
    add_library(lmdb::lmdb ALIAS lmdb)
endif()
