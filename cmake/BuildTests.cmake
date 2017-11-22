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

if(NOT EXCLUDE_BOOST)
	find_package(Boost COMPONENTS system filesystem)
endif()

function(createTests testName)
   message ("-- Adding test: ${testName}")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/catch")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp-yaml-cpp-0.5.3/include")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/jsoncpp/include")
    target_include_directories(${testName} BEFORE PRIVATE "${CIVET_THIRDPARTY_ROOT}/include")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libarchive-3.3.2/libarchive")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/include")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/c2/protocols")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/c2")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/controller")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/repository")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/yaml")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/statemanagement")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/statemanagement/metrics")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/io")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/utils")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/processors")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/provenance")
    if (Boost_FOUND)
        target_include_directories(${testName} BEFORE PRIVATE "${Boost_INCLUDE_DIRS}")
    endif()
    target_link_libraries(${testName} ${SPD_LIB} ${TEST_BASE_LIB})
    target_link_libraries(${testName} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} core-minifi yaml-cpp c-library civetweb-cpp ${JSON_CPP_LIB} dl)
    if (APPLE)
		target_link_libraries (${testName} -Wl,-all_load minifi)
	else ()
		target_link_libraries (${testName} -Wl,--whole-archive minifi -Wl,--no-whole-archive)
	endif ()
    if (Boost_FOUND)
        target_link_libraries(${testName} ${Boost_SYSTEM_LIBRARY})
        target_link_libraries(${testName} ${Boost_FILESYSTEM_LIBRARY})
    endif()
endfunction()

enable_testing(test)

SET(SPD_LIB spd_lib)
add_library(${SPD_LIB} STATIC ${SPD_SOURCES})

SET(TEST_BASE_LIB test_base)
add_library(${TEST_BASE_LIB} STATIC "${TEST_DIR}/TestBase.cpp")
target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/catch")
target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include")
target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CIVET_THIRDPARTY_ROOT}/include")
 
SET(CATCH_MAIN_LIB catch_main)
add_library(${CATCH_MAIN_LIB} STATIC "${TEST_DIR}/CatchMain.cpp")
target_include_directories(${CATCH_MAIN_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/catch")

SET(TEST_RESOURCES ${TEST_DIR}/resources)

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")
GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
  get_filename_component(testfilename "${testfile}" NAME_WE)
  add_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}")
  createTests("${testfilename}")
  target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
  MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
  add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")

SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
  get_filename_component(testfilename "${testfile}" NAME_WE)
  add_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}")
  createTests("${testfilename}")
  MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
foreach(EXTENSION ${extensions})
	add_subdirectory(${EXTENSION})
endforeach()

add_test(NAME TestExecuteProcess COMMAND TestExecuteProcess )
