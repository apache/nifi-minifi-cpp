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

set( CMAKE_VERBOSE_MAKEFILE on )

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_library(minifi-opc-extensions SHARED ${SOURCES})

target_link_libraries(minifi-opc-extensions ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-opc-extensions ${CMAKE_DL_LIBS} spdlog open62541::open62541)


SET (OPC-EXTENSIONS minifi-opc-extensions PARENT_SCOPE)

register_extension(minifi-opc-extensions)
register_extension_linter(minifi-opc-extensions-linter)
