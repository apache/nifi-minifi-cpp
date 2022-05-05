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

if (NOT ENABLE_TENSORFLOW)
  return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(TensorFlow REQUIRED)

message("-- Found TensorFlow: ${TENSORFLOW_INCLUDE_DIRS}")

file(GLOB SOURCES  "*.cpp")

add_library(minifi-tensorflow-extensions SHARED ${SOURCES})

target_include_directories(minifi-tensorflow-extensions SYSTEM PRIVATE ${TENSORFLOW_INCLUDE_DIRS})
target_link_libraries(minifi-tensorflow-extensions ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-tensorflow-extensions ${TENSORFLOW_LIBRARIES})

SET (TENSORFLOW-EXTENSIONS minifi-tensorflow-extensions PARENT_SCOPE)

register_extension(minifi-tensorflow-extensions "TENSORFLOW EXTENSIONS" TENSORFLOW-EXTENSIONS "This enables TensorFlow support" "${TEST_DIR}/tensorflow-tests")

