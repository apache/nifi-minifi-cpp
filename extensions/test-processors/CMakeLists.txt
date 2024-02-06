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

if (SKIP_TESTS AND NOT ENABLE_TEST_PROCESSORS)
    return()
endif()

include(../ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-test-processors SHARED "${SOURCES}")

target_link_libraries(minifi-test-processors ${LIBMINIFI})

register_extension(minifi-test-processors "TEST-PROCESSORS EXTENSION" TEST-PROCESSORS-EXTENSION "This enables processors purely for testing purposes" "" "CI_ONLY_INSTALL")

register_extension_linter(minifi-test-processors-extensions-linter)
