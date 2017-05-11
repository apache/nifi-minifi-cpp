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
    IF( "${curdir}/${child}" MATCHES .*\\.cpp)
  
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

function(createTests testName)
   message ("-- Adding test: ${testName}")
    target_include_directories(${testName} PRIVATE BEFORE ${UUID_INCLUDE_DIRS})
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/catch")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/spdlog-0.13.0/include")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(${testName} PRIVATE BEFORE "thirdparty/jsoncpp/include")
    target_include_directories(${testName} PRIVATE BEFORE ${LEVELDB_INCLUDE_DIRS})
    target_include_directories(${testName} PRIVATE BEFORE "include")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/controller")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/repository")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/core/yaml")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/io")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/utils")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/processors")
    target_include_directories(${testName} PRIVATE BEFORE "libminifi/include/provenance")
    target_link_libraries(${testName} ${CMAKE_THREAD_LIBS_INIT} ${UUID_LIBRARIES} ${LEVELDB_LIBRARIES} ${OPENSSL_LIBRARIES} minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB})
     if (CURL_FOUND)
        target_include_directories(${testName} PRIVATE BEFORE ${CURL_INCLUDE_DIRS})
                target_link_libraries(${testName} ${CURL_LIBRARIES})
        endif(CURL_FOUND)
endfunction()


enable_testing(test)

SET(TEST_DIR ${CMAKE_SOURCE_DIR}/libminifi/test)
SET(TEST_RESOURCES ${TEST_DIR}/resources)

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")
GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}" ${SPD_SOURCES})
	createTests("${testfilename}")
 	MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")

SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}" ${SPD_SOURCES})
	createTests("${testfilename}")
	#message("Adding ${testfilename} from ${testfile}")
	MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

add_test(NAME ControllerServiceIntegrationTests COMMAND ControllerServiceIntegrationTests "${TEST_RESOURCES}/TestControllerServices.yml" "${TEST_RESOURCES}/")

add_test(NAME HttpGetIntegrationTest COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGet.yml"  "${TEST_RESOURCES}/")

add_test(NAME HttpGetIntegrationTestSecure COMMAND HttpGetIntegrationTest "${TEST_RESOURCES}/TestHTTPGetSecure.yml"  "${TEST_RESOURCES}/")

add_test(NAME HttpPostIntegrationTest COMMAND HttpPostIntegrationTest "${TEST_RESOURCES}/TestHTTPPost.yml" )

add_test(NAME TestExecuteProcess COMMAND TestExecuteProcess )
