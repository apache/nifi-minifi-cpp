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


### test functions
MACRO(GETSOURCEFILES result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF( "${child}" MATCHES ^[^.].*\\.cpp)
  
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

function(createTests testName)
   message ("-- Adding test: ${testName}")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/catch")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/spdlog-20170710/include")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/jsoncpp/include")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/civetweb-1.9.1/include")
    target_include_directories(${testName} PRIVATE BEFORE "include")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/c2/protocols")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/c2")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/controller")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/repository")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/yaml")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/statemanagement")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/statemanagement/metrics")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/io")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/utils")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/processors")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/provenance")
    target_link_libraries(${testName} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB})
     if (CURL_FOUND)
        target_include_directories(${testName} PRIVATE BEFORE ${CURL_INCLUDE_DIRS})
        target_include_directories(${testName} PRIVATE BEFORE "extensions/http-curl/")
        target_include_directories(${testName} PRIVATE BEFORE "extensions/http-curl/client/")
        target_include_directories(${testName} PRIVATE BEFORE "extensions/http-curl/processors/")
        target_include_directories(${testName} PRIVATE BEFORE "extensions/http-curl/protocols/")
        target_link_libraries(${testName} ${CURL_LIBRARIES} )
        if (APPLE)    
		    target_link_libraries (${testName} -Wl,-all_load ${HTTP-CURL})
		else ()
			target_link_libraries (${testName} -Wl,--whole-archive ${HTTP-CURL} -Wl,--no-whole-archive)    
		endif ()
    endif()
endfunction()


enable_testing(test)

SET(TEST_DIR ${CMAKE_SOURCE_DIR}/libminifi/test)
SET(TEST_RESOURCES ${TEST_DIR}/resources)

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")
GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")
GETSOURCEFILES(CURL_INTEGRATION_TESTS "${TEST_DIR}/curl-tests/")
GETSOURCEFILES(ROCKSDB_INTEGRATION_TESTS "${TEST_DIR}/rocksdb-tests/")

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}" ${SPD_SOURCES} "${TEST_DIR}/TestBase.cpp")
	createTests("${testfilename}")
 	MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")

SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}" ${SPD_SOURCES} "${TEST_DIR}/TestBase.cpp")
	createTests("${testfilename}")
	MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

if (ROCKSDB-REPOS)
SET(ROCKSDB_TEST_COUNT 0)
FOREACH(testfile ${ROCKSDB_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/rocksdb-tests/${testfile}" ${SPD_SOURCES} "${TEST_DIR}/TestBase.cpp")
	target_include_directories(${testfilename} PRIVATE BEFORE "extensions/rocksdb-repos/")
    target_include_directories(${testfilename} PRIVATE BEFORE "thirdparty/rocksdb/include")
    if (APPLE)    
	    	target_link_libraries (${testfilename} -Wl,-all_load ${ROCKSDB-REPOS})
	else ()
			target_link_libraries (${testfilename} -Wl,--whole-archive ${ROCKSDB-REPOS} -Wl,--no-whole-archive)    
	endif ()
	createTests("${testfilename}")
	MATH(EXPR ROCKSDB_TEST_COUNT "${ROCKSDB_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${ROCKSDB_TEST_COUNT} RocksDB related test file(s)...")
endif(ROCKSDB-REPOS)

if (HTTP-CURL)

SET(CURL_INT_TEST_COUNT 0)
FOREACH(testfile ${CURL_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/curl-tests/${testfile}" ${SPD_SOURCES} "${TEST_DIR}/TestBase.cpp")
	createTests("${testfilename}")
	#message("Adding ${testfilename} from ${testfile}")
	MATH(EXPR CURL_INT_TEST_COUNT "${CURL_INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${CURL_INT_TEST_COUNT} libcURL integration test file(s)...")



add_test(NAME HttpGetIntegrationTest COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")

add_test(NAME C2UpdateTest COMMAND C2UpdateTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")

add_test(NAME HttpGetIntegrationTestSecure COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGetSecure.yml"  "${TEST_RESOURCES}/")

add_test(NAME HttpPostIntegrationTest COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPost.yml" "${TEST_RESOURCES}/")

add_test(NAME HttpPostIntegrationTestChunked COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")

add_test(NAME C2VerifyServeResults COMMAND C2VerifyServeResults "${TEST_RESOURCES}/TestHTTPGet.yml" "${TEST_RESOURCES}/")

add_test(NAME C2VerifyHeartbeatAndStop COMMAND C2VerifyHeartbeatAndStop "${TEST_RESOURCES}/TestHTTPGet.yml" "${TEST_RESOURCES}/")

add_test(NAME SiteToSiteRestTest COMMAND SiteToSiteRestTest "${TEST_RESOURCES}/TestSite2SiteRest.yml" "${TEST_RESOURCES}/" "http://localhost:8082/nifi-api/controller")

## removed due to travis issues with our certs
#add_test(NAME SiteToSiteRestTestSecure COMMAND SiteToSiteRestTest "${TEST_RESOURCES}/TestSite2SiteRestSecure.yml" "${TEST_RESOURCES}/" "https://localhost:8082/nifi-api/controller")

add_test(NAME ControllerServiceIntegrationTests COMMAND ControllerServiceIntegrationTests "${TEST_RESOURCES}/TestControllerServices.yml" "${TEST_RESOURCES}/")

add_test(NAME ThreadPoolAdjust COMMAND ThreadPoolAdjust "${TEST_RESOURCES}/TestHTTPPostChunkedEncoding.yml" "${TEST_RESOURCES}/")

endif(HTTP-CURL)

add_test(NAME TestExecuteProcess COMMAND TestExecuteProcess )
