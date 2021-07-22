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

file(GLOB CURL_UNIT_TESTS  "unit/*.cpp")
file(GLOB CURL_INTEGRATION_TESTS "*.cpp")

SET(CURL_INT_TEST_COUNT 0)

FOREACH(testfile ${CURL_UNIT_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../")
	target_include_directories(${testfilename} BEFORE PRIVATE "../client/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../protocols/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../sitetosite/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
	target_include_directories(${testfilename} BEFORE PRIVATE ./include)
	createTests("${testfilename}")
	target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
	target_link_libraries(${testfilename} minifi-http-curl)
	target_link_libraries(${testfilename} minifi-civet-extensions)
	target_link_libraries(${testfilename} minifi-standard-processors)

	MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()

FOREACH(testfile ${CURL_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../")
	target_include_directories(${testfilename} BEFORE PRIVATE "../client/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../protocols/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../sitetosite/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
	target_include_directories(${testfilename} BEFORE PRIVATE ./include)
	createTests("${testfilename}")
	target_link_libraries(${testfilename} minifi-http-curl)
	target_link_libraries(${testfilename} minifi-civet-extensions)
	target_link_libraries(${testfilename} minifi-standard-processors)

	MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${CURL_INT_TEST_COUNT} libcURL integration test file(s)...")

add_test(NAME VerifyInvokeHTTPGetTest COMMAND VerifyInvokeHTTPGetTest "${TEST_RESOURCES}/TestHTTPGet.yml")
add_test(NAME C2UpdateTest COMMAND C2UpdateTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2FetchFlowIfMissingTest COMMAND C2FetchFlowIfMissingTest "${TEST_RESOURCES}/TestEmpty.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2ConfigEncryption COMMAND C2ConfigEncryption "${TEST_RESOURCES}/decrypted.config.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2JstackTest COMMAND C2JstackTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2DescribeManifestTest COMMAND C2DescribeManifestTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2DescribeCoreComponentStateTest COMMAND C2DescribeCoreComponentStateTest "${TEST_RESOURCES}/TestC2DescribeCoreComponentState.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2FailedUpdateTest COMMAND C2FailedUpdateTest "${TEST_RESOURCES}/TestHTTPGet.yml" "${TEST_RESOURCES}/" "${TEST_RESOURCES}/TestBad.yml")
add_test(NAME C2NullConfiguration COMMAND C2NullConfiguration "${TEST_RESOURCES}/TestNull.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2RequestClassTest COMMAND C2RequestClassTest)
if (NOT OPENSSL_OFF)
	add_test(NAME VerifyInvokeHTTPGetTestSecure COMMAND VerifyInvokeHTTPGetTest "${TEST_RESOURCES}/TestHTTPGetSecure.yml"  "${TEST_RESOURCES}/")
	add_test(NAME C2VerifyHeartbeatAndStopSecure COMMAND C2VerifyHeartbeatAndStop "${TEST_RESOURCES}/C2VerifyHeartbeatAndStopSecure.yml" "${TEST_RESOURCES}/")
	add_test(NAME VerifyInvokeHTTPPostTestSecure COMMAND VerifyInvokeHTTPPostTest "${TEST_RESOURCES}/TestInvokeHTTPPostSecure.yml" "${TEST_RESOURCES}/")
endif()
add_test(NAME HttpPostIntegrationTest COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPost.yml" "${TEST_RESOURCES}/")
if (NOT APPLE)
	add_test(NAME HttpPostIntegrationTestChunked COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")
endif()
add_test(NAME C2VerifyServeResults COMMAND C2VerifyServeResults "${TEST_RESOURCES}/C2VerifyServeResults.yml" "${TEST_RESOURCES}/")
add_test(NAME C2VerifyHeartbeatAndStop COMMAND C2VerifyHeartbeatAndStop "${TEST_RESOURCES}/C2VerifyHeartbeatAndStop.yml" )
add_test(NAME C2VerifyResourceConsumptionInHeartbeat COMMAND C2VerifyResourceConsumptionInHeartbeat "${TEST_RESOURCES}/C2VerifyHeartbeatAndStop.yml")
add_test(NAME HTTPSiteToSiteTests COMMAND HTTPSiteToSiteTests "${TEST_RESOURCES}/TestHTTPSiteToSite.yml" "${TEST_RESOURCES}/" "http://localhost:8099/nifi-api")
add_test(NAME TimeoutHTTPSiteToSiteTests COMMAND TimeoutHTTPSiteToSiteTests "${TEST_RESOURCES}/TestTimeoutHTTPSiteToSite.yml" "${TEST_RESOURCES}/" "http://localhost:8098/nifi-api")
add_test(NAME SiteToSiteRestTest COMMAND SiteToSiteRestTest "${TEST_RESOURCES}/TestSite2SiteRest.yml" "${TEST_RESOURCES}/" "http://localhost:8077/nifi-api/site-to-site")
add_test(NAME ControllerServiceIntegrationTests COMMAND ControllerServiceIntegrationTests "${TEST_RESOURCES}/TestControllerServices.yml" "${TEST_RESOURCES}/")
add_test(NAME VerifyInvokeHTTPPostTest COMMAND VerifyInvokeHTTPPostTest "${TEST_RESOURCES}/TestInvokeHTTPPost.yml")
add_test(NAME AbsoluteTimeoutTest COMMAND AbsoluteTimeoutTest)
add_test(NAME C2PauseResumeTest COMMAND C2PauseResumeTest "${TEST_RESOURCES}/C2PauseResumeTest.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2LogHeartbeatTest COMMAND C2LogHeartbeatTest)
