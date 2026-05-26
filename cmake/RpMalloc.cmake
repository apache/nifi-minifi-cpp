#
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
#
include(FetchContent)
FetchContent_Declare(
        rpmalloc
        URL      https://github.com/mjansson/rpmalloc/archive/refs/tags/1.4.5.tar.gz
        URL_HASH SHA256=2513626697ef72a60957acc8caed17c39931a55c1a49202707de195742683d69
        SYSTEM
)
FetchContent_GetProperties(rpmalloc)

if(NOT rpmalloc_POPULATED)
    FetchContent_Populate(rpmalloc)
    add_library(rpmalloc ${rpmalloc_SOURCE_DIR}/rpmalloc/rpmalloc.c)
    target_include_directories(rpmalloc PUBLIC ${rpmalloc_SOURCE_DIR}/rpmalloc)
    target_compile_definitions(rpmalloc PRIVATE ENABLE_OVERRIDE=1 ENABLE_PRELOAD=1)
endif()
