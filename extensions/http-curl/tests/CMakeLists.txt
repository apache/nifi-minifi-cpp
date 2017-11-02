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

file(GLOB CURL_INTEGRATION_TESTS  "*.cpp")
file(GLOB CURL_UNIT_TESTS  "unit/*.cpp")

SET(CURL_TEST_COUNT 0)


FOREACH(testfile ${CURL_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE ${CURL_INCLUDE_DIRS})
    target_include_directories(${testfilename} PRIVATE BEFORE ../include/)
    target_link_libraries(${testfilename} ${CURL_LIBRARIES})
    createTests("${testfilename}")
    if(APPLE)
        target_link_libraries("${testfilename}" -Wl,-all_load minifi-http-curl)
    else()
        target_link_libraries("${testfilename}" -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive)
    endif()
    MATH(EXPR CURL_TEST_COUNT "${CURL_TEST_COUNT}+1")
ENDFOREACH()

FOREACH(testfile ${CURL_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE ${CURL_INCLUDE_DIRS})
    target_include_directories(${testfilename} PRIVATE BEFORE ../include/)
    target_link_libraries(${testfilename} ${CURL_LIBRARIES})
    createTests("${testfilename}")
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    if(APPLE)
        target_link_libraries("${testfilename}" -Wl,-all_load minifi-http-curl)
    else()
        target_link_libraries("${testfilename}" -Wl,--whole-archive minifi-http-curl -Wl,--no-whole-archive)
    endif()
    MATH(EXPR CURL_TEST_COUNT "${CURL_TEST_COUNT}+1")
ENDFOREACH()


message("-- Finished building ${CURL_TEST_COUNT} libcURL test file(s)...")

set(LOCAL_RESOURCES "${CMAKE_CURRENT_LIST_DIR}/resources")
add_test(NAME HttpGetIntegrationTest COMMAND HttpGetIntegrationTest "${LOCAL_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2UpdateTest COMMAND C2UpdateTest "${LOCAL_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")
add_test(NAME C2NullConfiguration COMMAND C2NullConfiguration "${LOCAL_RESOURCES}/TestNull.yml"  "${TEST_RESOURCES}/")
add_test(NAME HttpGetIntegrationTestSecure COMMAND HttpGetIntegrationTest "${LOCAL_RESOURCES}/TestHTTPGetSecure.yml"  "${TEST_RESOURCES}/")
add_test(NAME HttpPostIntegrationTest COMMAND HttpPostIntegrationTest "${LOCAL_RESOURCES}/TestHTTPPost.yml" "${TEST_RESOURCES}/")
add_test(NAME HttpPostIntegrationTestChunked COMMAND HttpPostIntegrationTest "${LOCAL_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")
add_test(NAME C2VerifyServeResults COMMAND C2VerifyServeResults "${LOCAL_RESOURCES}/C2VerifyServeResults.yml" "${TEST_RESOURCES}/")
add_test(NAME C2VerifyHeartbeatAndStop COMMAND C2VerifyHeartbeatAndStop "${LOCAL_RESOURCES}/C2VerifyHeartbeatAndStop.yml" "${TEST_RESOURCES}/")
add_test(NAME SiteToSiteRestTest COMMAND SiteToSiteRestTest "${LOCAL_RESOURCES}/TestSite2SiteRest.yml" "${TEST_RESOURCES}/" "http://localhost:8077/nifi-api/site-to-site")
add_test(NAME ControllerServiceIntegrationTests COMMAND ControllerServiceIntegrationTests "${LOCAL_RESOURCES}/TestControllerServices.yml" "${TEST_RESOURCES}/")
add_test(NAME ThreadPoolAdjust COMMAND ThreadPoolAdjust "${LOCAL_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")
