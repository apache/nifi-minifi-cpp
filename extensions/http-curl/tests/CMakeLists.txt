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
  	target_include_directories(${testfilename} BEFORE PRIVATE ${CURL_INCLUDE_DIRS})
  	target_include_directories(${testfilename} BEFORE PRIVATE "${CIVET_THIRDPARTY_ROOT}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../")
	target_include_directories(${testfilename} BEFORE PRIVATE "../client/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../protocols/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../sitetosite/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
	target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    createTests("${testfilename}")
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    if (APPLE)
    	target_link_libraries ("${testfilename}" -Wl,-all_load ${ZLIB_LIBRARY} ${OPENSSL_LIBRARIES} minifi-http-curl minifi-civet-extensions)
	else ()
  		target_link_libraries ("${testfilename}" -Wl,--whole-archive ${ZLIB_LIBRARY} ${OPENSSL_LIBRARIES} minifi-http-curl minifi-civet-extensions -Wl,--no-whole-archive)
  	endif()
  MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
ENDFOREACH()

FOREACH(testfile ${CURL_INTEGRATION_TESTS})
  	get_filename_component(testfilename "${testfile}" NAME_WE)
  	add_executable("${testfilename}" "${testfile}")
  	target_include_directories(${testfilename} BEFORE PRIVATE ${CURL_INCLUDE_DIRS})
  	target_include_directories(${testfilename} BEFORE PRIVATE "${CIVET_THIRDPARTY_ROOT}/include")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../")
	target_include_directories(${testfilename} BEFORE PRIVATE "../client/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../protocols/")
	target_include_directories(${testfilename} BEFORE PRIVATE "../sitetosite/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/civetweb/")
	target_include_directories(${testfilename} BEFORE PRIVATE ./include)
        if (APPLE)
    	target_link_libraries ("${testfilename}" -Wl,-all_load ${ZLIB_LIBRARY} ${OPENSSL_LIBRARIES} minifi-http-curl minifi-civet-extensions)
	else ()
  		target_link_libraries ("${testfilename}" -Wl,--whole-archive ${ZLIB_LIBRARY} ${OPENSSL_LIBRARIES} minifi-http-curl minifi-civet-extensions -Wl,--no-whole-archive)
  	endif()
    createTests("${testfilename}")
  MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${CURL_INT_TEST_COUNT} libcURL integration test file(s)...")

add_test(NAME HttpGetIntegrationTest COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2UpdateTest COMMAND C2UpdateTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2JstackTest COMMAND C2JstackTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2UpdateAgentTest COMMAND C2UpdateAgentTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2FailedUpdateTest COMMAND C2FailedUpdateTest "${TEST_RESOURCES}/TestHTTPGet.yml" "${TEST_RESOURCES}/TestBad.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2NullConfiguration COMMAND C2NullConfiguration "${TEST_RESOURCES}/TestNull.yml"  "${TEST_RESOURCES}/")
#add_test(NAME HttpGetIntegrationTestSecure COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGetSecure.yml"  "${TEST_RESOURCES}/")
add_test(NAME HttpPostIntegrationTest COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPost.yml" "${TEST_RESOURCES}/")
add_test(NAME HttpPostIntegrationTestChunked COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")
add_test(NAME C2VerifyServeResults COMMAND C2VerifyServeResults "${TEST_RESOURCES}/C2VerifyServeResults.yml" "${TEST_RESOURCES}/")
add_test(NAME C2VerifyHeartbeatAndStop COMMAND C2VerifyHeartbeatAndStop "${TEST_RESOURCES}/C2VerifyHeartbeatAndStop.yml" "${TEST_RESOURCES}/")
add_test(NAME HTTPSiteToSiteTests COMMAND HTTPSiteToSiteTests "${TEST_RESOURCES}/TestHTTPSiteToSite.yml" "${TEST_RESOURCES}/" "http://localhost:8099/nifi-api")
add_test(NAME SiteToSiteRestTest COMMAND SiteToSiteRestTest "${TEST_RESOURCES}/TestSite2SiteRest.yml" "${TEST_RESOURCES}/" "http://localhost:8077/nifi-api/site-to-site")
add_test(NAME ControllerServiceIntegrationTests COMMAND ControllerServiceIntegrationTests "${TEST_RESOURCES}/TestControllerServices.yml" "${TEST_RESOURCES}/")
add_test(NAME ThreadPoolAdjust COMMAND ThreadPoolAdjust "${TEST_RESOURCES}/ThreadPoolAdjust.yml" "${TEST_RESOURCES}/")
