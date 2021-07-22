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

file(GLOB COAP_INTEGRATION_TESTS "*.cpp")

SET(CURL_INT_TEST_COUNT 0)

if (NOT DISABLE_CURL)
FOREACH(testfile ${COAP_INTEGRATION_TESTS})
  	get_filename_component(testfilename "${testfile}" NAME_WE)
  	add_executable("${testfilename}" "${testfile}")
  	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../")
	target_include_directories(${testfilename} BEFORE PRIVATE "../server")
	target_include_directories(${testfilename} BEFORE PRIVATE "../nanofi")
	target_include_directories(${testfilename} BEFORE PRIVATE "../protocols")
	target_include_directories(${testfilename} BEFORE PRIVATE "../controllerservice")
	target_include_directories(${testfilename} BEFORE PRIVATE "../../http-curl/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../../http-curl/client/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../../http-curl/processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../../http-curl/protocols/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../../http-curl/sitetosite/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
	target_include_directories(${testfilename} BEFORE PRIVATE ./include)
	createTests("${testfilename}")
	target_link_libraries(${testfilename} minifi-coap)
	target_link_libraries(${testfilename} minifi-civet-extensions)
	target_link_libraries(${testfilename} minifi-http-curl)
	target_link_libraries(${testfilename} minifi-standard-processors)

    MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${CURL_INT_TEST_COUNT} CoAP integration test file(s)...")

add_test(NAME CoapC2VerifyHeartbeat COMMAND CoapC2VerifyHeartbeat "${TEST_RESOURCES}/CoapC2VerifyServe.yml" "${TEST_RESOURCES}/" "http://localhost:9888/geturl")
else()
message("-- CoAP tests rely on minifi-http-curl, so they will not be built...")
endif()
