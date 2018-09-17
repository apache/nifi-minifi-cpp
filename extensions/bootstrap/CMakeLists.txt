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

cmake_minimum_required(VERSION 2.6)

include_directories(../../libminifi/include ../thirdparty/cxxopts/include)

set(BOOTSTRAP_SOURCES  bootstrap.cpp)
add_executable(bstrp ${BOOTSTRAP_SOURCES})

target_link_libraries(bstrp cxxopts)

set_target_properties(bstrp
        PROPERTIES OUTPUT_NAME bstrp)


add_custom_target(bootstrap-docs
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bstrp --inputc2docs "${CMAKE_SOURCE_DIR}/PROCESSORS.md" --outputc2docs "${CMAKE_SOURCE_DIR}/libminifi/include/agent/agent_docs.h"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generates documentation ${CMAKE_CURRENT_SOURCE_DIR}"
  SOURCES ${BOOTSTRAP_SOURCES}
)

execute_process(COMMAND bootstrap-docs)

