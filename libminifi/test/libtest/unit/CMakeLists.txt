# Licensed to the Apache Software Foundation (ASF) under one
#
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
file(GLOB TEST_BASE_SOURCES "*.cpp")
add_minifi_library(libminifi-unittest STATIC ${TEST_BASE_SOURCES})
target_link_libraries(libminifi-unittest core-minifi minifi-extension-utils Catch2)
target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
if(WIN32)
    target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/win")
else()
    target_include_directories(libminifi-unittest BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/posix")
endif()
