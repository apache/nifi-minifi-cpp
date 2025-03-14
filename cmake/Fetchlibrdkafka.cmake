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
include(Zstd)

include(LZ4)

set(WITH_SSL "ON" CACHE STRING "" FORCE)
set(WITH_SASL "OFF" CACHE STRING "" FORCE)
set(WITH_ZSTD "ON" CACHE STRING "" FORCE)
set(WITH_SNAPPY "ON" CACHE STRING "" FORCE)
set(ENABLE_LZ4_EXT "ON" CACHE STRING "" FORCE)
set(RDKAFKA_BUILD_STATIC "ON" CACHE STRING "" FORCE)
set(RDKAFKA_BUILD_EXAMPLES "OFF" CACHE STRING "" FORCE)
set(RDKAFKA_BUILD_TESTS "OFF" CACHE STRING "" FORCE)
set(LIBRDKAFKA_STATICLIB "1" CACHE STRING "" FORCE)

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/librdkafka/0001-remove-findLZ4-and-findZSTD.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/librdkafka/0002-undef-X509_NAME.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

FetchContent_Declare(libkafka
        URL https://github.com/confluentinc/librdkafka/archive/refs/tags/v2.8.0.tar.gz
        URL_HASH SHA256=5bd1c46f63265f31c6bfcedcde78703f77d28238eadf23821c2b43fc30be3e25
        PATCH_COMMAND "${PC}"
)

FetchContent_MakeAvailable(libkafka)

get_target_property(ZSTD_INCLUDE_DIRS zstd::zstd INCLUDE_DIRECTORIES)
get_target_property(LZ4_INCLUDE_DIRS lz4::lz4 INCLUDE_DIRECTORIES)

target_include_directories(rdkafka SYSTEM PRIVATE ${ZSTD_INCLUDE_DIRS})
target_include_directories(rdkafka SYSTEM PRIVATE ${LZ4_INCLUDE_DIRS})

target_link_libraries(rdkafka INTERFACE zstd::zstd lz4::lz4)
