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

file(GLOB MQTT_TESTS  "*.cpp")

FOREACH(testfile ${MQTT_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_minifi_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/../../extensions/standard-processors")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/../../../libminifi/test/")
	createTests("${testfilename}")
	target_link_libraries(${testfilename} Catch2WithMain)
	target_link_libraries(${testfilename} minifi-mqtt-extensions)
	target_link_libraries(${testfilename} minifi-standard-processors)
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
ENDFOREACH()

list(LENGTH MQTT_TESTS TEST_COUNT)
message("-- Finished building ${TEST_COUNT} MQTT related test file(s)...")
