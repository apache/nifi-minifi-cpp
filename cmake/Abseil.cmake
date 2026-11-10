# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL absl-propagate-cxx)
set(ABSL_ENABLE_INSTALL ON CACHE INTERNAL "")
set(ABSL_USE_SYSTEM_INCLUDES ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE STRING "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/abseil/rename-crc32.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(
        absl
        URL      https://github.com/abseil/abseil-cpp/archive/refs/tags/20260817.0.tar.gz
        URL_HASH SHA256=f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d
        PATCH_COMMAND "${PC}"
        OVERRIDE_FIND_PACKAGE
        SYSTEM
)
FetchContent_MakeAvailable(absl)
