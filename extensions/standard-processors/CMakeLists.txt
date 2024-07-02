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


include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "processors/*.cpp" "controllers/*.cpp" "utils/*.cpp" "modbus/*.cpp")

add_minifi_library(minifi-standard-processors SHARED ${SOURCES})
target_include_directories(minifi-standard-processors PUBLIC "${CMAKE_SOURCE_DIR}/extensions/standard-processors")

include(RangeV3)
include(Asio)
target_link_libraries(minifi-standard-processors ${LIBMINIFI} Threads::Threads range-v3 asio)

include(Coroutines)
enable_coroutines()

register_extension(minifi-standard-processors "STANDARD PROCESSORS" STANDARD-PROCESSORS "Provides standard processors" "extensions/standard-processors/tests/")

register_extension_linter(minifi-standard-processors-linter)
