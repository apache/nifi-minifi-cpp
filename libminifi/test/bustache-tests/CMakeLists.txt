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

file(GLOB BUSTACHE_INTEGRATION_TESTS "*.cpp")
SET(BUSTACHE-EXTENSIONS_TEST_COUNT 0)
find_package(Boost COMPONENTS iostreams REQUIRED)
FOREACH(testfile ${BUSTACHE_INTEGRATION_TESTS})
  get_filename_component(testfilename "${testfile}" NAME_WE)
  add_executable("${testfilename}" "${testfile}")
  target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/bustache")
  target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
  target_include_directories(${testfilename} SYSTEM PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/bustache/include")

  target_link_libraries(${testfilename} minifi-bustache-extensions)
  target_link_libraries(${testfilename} minifi-standard-processors)

  createTests("${testfilename}")
  target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
  target_link_libraries(${testfilename} ${Boost_IOSTREAMS_LIBRARY})
  MATH(EXPR BUSTACHE-EXTENSIONS_TEST_COUNT "${BUSTACHE-EXTENSIONS_TEST_COUNT}+1")
  add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${BUSTACHE-EXTENSIONS_TEST_COUNT} Bustache related test file(s)...")
