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

# Find the preferred Apache mirror to use for the download by querying the list of mirrors and filtering out 'preferred'
execute_process(COMMAND curl -s https://www.apache.org/dyn/closer.lua/?asjson=1
        COMMAND grep preferred
        COMMAND awk "{print $2}"
        COMMAND tr -d "\""
        TIMEOUT 10
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE MIRROR_URL )

ExternalProject_Add(
        rat-binary
        PREFIX "apache-rat"
        URL "${MIRROR_URL}creadur/apache-rat-0.12/apache-rat-0.12-bin.tar.gz"
        URL_HASH SHA1=e84dffe8b354871c29f5078b823a726508474a6c
        DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

# Custom target to run Apache Release Audit Tool (RAT)
add_custom_target(
        apache-rat
        COMMAND java -jar ${CMAKE_BINARY_DIR}/apache-rat/src/rat-binary/apache-rat-0.12.jar -E ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat/.rat-excludes -d ${CMAKE_SOURCE_DIR} | grep -B 1 -A 15 Summary )


add_dependencies(apache-rat rat-binary)
