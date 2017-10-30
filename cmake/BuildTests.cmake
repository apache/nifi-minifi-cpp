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

find_package(Boost COMPONENTS system filesystem REQUIRED)

function(createTests testName)
   message ("-- Adding test: ${testName}")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/catch")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/jsoncpp/include")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/civetweb-1.9.1/include")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/libarchive-3.3.2/libarchive")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/include")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/c2/protocols")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/c2")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core/controller")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core/repository")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core/yaml")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core/statemanagement")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/core/statemanagement/metrics")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/io")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/utils")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/processors")
    target_include_directories(${testName} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/include/provenance")
    target_link_libraries(${testName} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} core-minifi minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB})
    target_link_libraries(${testName} ${Boost_SYSTEM_LIBRARY})
    target_link_libraries(${testName} ${Boost_FILESYSTEM_LIBRARY})
endfunction()


enable_testing(test)

SET(TEST_RESOURCES ${TEST_DIR}/resources)

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")
GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

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


get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
foreach(EXTENSION ${extensions})
	message("Adding ${EXTENSION} ? ")
	add_subdirectory(${EXTENSION})
endforeach()

add_test(NAME TestExecuteProcess COMMAND TestExecuteProcess )
