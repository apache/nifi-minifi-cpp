# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(PATCH_FILE "${CMAKE_SOURCE_DIR}/thirdparty/liblzma/liblzma.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE}\\\")")

FetchContent_Declare(liblzma
        URL https://github.com/tukaani-project/xz/releases/download/v5.6.2/xz-5.6.2.tar.gz
        URL_HASH SHA256=8bfd20c0e1d86f0402f2497cfa71c6ab62d4cd35fd704276e3140bfb71414519
        PATCH_COMMAND "${PC}"
        SYSTEM)

FetchContent_MakeAvailable(liblzma)

add_library(LibLZMA::LibLZMA ALIAS liblzma)

# Set exported variables for FindPackage.cmake

set(LIBLZMA_INCLUDE_DIR "${liblzma_SOURCE_DIR}/src/liblzma/api" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBLZMA_INCLUDE_DIR=${LIBLZMA_INCLUDE_DIR}" CACHE STRING "" FORCE)

if (WIN32)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBLZMA_LIB_DIR=${liblzma_BINARY_DIR}" CACHE STRING "" FORCE)
else()
    set(LIBLZMA_LIBRARY "${liblzma_BINARY_DIR}/liblzma.a" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_LIBLZMA_LIBRARY=${LIBLZMA_LIBRARY}" CACHE STRING "" FORCE)
endif()
