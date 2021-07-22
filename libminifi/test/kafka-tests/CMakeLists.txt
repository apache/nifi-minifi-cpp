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

file(GLOB KAFKA_INTEGRATION_TESTS  "*.cpp")

SET(EXTENSIONS_TEST_COUNT 0)
FOREACH(testfile ${KAFKA_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/librdkafka")
	target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/librdkafka-0.11.4/src")
	target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/librdkafka-0.11.4/src-cpp")
	createTests("${testfilename}")
	target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
	target_link_libraries(${testfilename} minifi-rdkafka-extensions)
	target_link_libraries(${testfilename} minifi-standard-processors)
	MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${KAFKA-EXTENSIONS_TEST_COUNT} Lib Kafka related test file(s)...")
