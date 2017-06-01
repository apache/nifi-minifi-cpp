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

# This file is invoked in CMake script mode from the source root of the overall project

# Find the preferred Apache mirror to use for the download by querying the list of mirrors and filtering out 'preferred'
execute_process(COMMAND curl -s https://www.apache.org/dyn/closer.lua/?asjson=1
        COMMAND grep preferred
        COMMAND awk "{print $2}"
        COMMAND tr -d "\""
        TIMEOUT 10
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE MIRROR_URL )

# Make use of parent thirdparty by adjusting relative to the source of this CMake file
set(PARENT_THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/../thirdparty")
set(RAT_BASENAME "apache-rat-0.12")
set(RAT_DIR "${PARENT_THIRDPARTY_DIR}/apache-rat")
set(RAT_BINARY "${RAT_DIR}/${RAT_BASENAME}-bin/${RAT_BASENAME}.jar")

file(DOWNLOAD
        "${MIRROR_URL}creadur/${RAT_BASENAME}/${RAT_BASENAME}-bin.tar.gz"
        "${RAT_DIR}/${RAT_BASENAME}-bin.tar.gz"
        EXPECTED_HASH SHA512=460d53fa3e1546d960bd03ebd97c930a39306cffd98c9ebc09bf22f9e50a9723578b98c4e7dc71dd6f19dfb6dae00811cb11a4eabce70c21502a6cef2d9fd2fa )

execute_process(
        COMMAND tar xf "${RAT_DIR}/${RAT_BASENAME}-bin.tar.gz" -C "${RAT_DIR}"
        COMMAND grep preferred
        COMMAND awk "{print $2}"
        COMMAND tr -d "\""
        TIMEOUT 10
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE MIRROR_URL )