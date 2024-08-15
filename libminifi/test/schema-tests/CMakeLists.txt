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

include(JsonSchemaValidator)

file(GLOB SCHEMA_TESTS  "*.cpp")
SET(SCHEMA_TEST_COUNT 0)
FOREACH(testfile ${SCHEMA_TESTS})
  get_filename_component(testfilename "${testfile}" NAME_WE)
  add_minifi_executable("${testfilename}" "${testfile}")
  createTests("${testfilename}")
  target_link_libraries(${testfilename} Catch2WithMain)
  target_link_libraries(${testfilename} minifi-standard-processors)
  target_link_libraries(${testfilename} nlohmann_json_schema_validator)
  MATH(EXPR SCHEMA_TEST_COUNT "${SCHEMA_TEST_COUNT}+1")
  add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${SCHEMA_TEST_COUNT} Json Schema related test file(s)...")
