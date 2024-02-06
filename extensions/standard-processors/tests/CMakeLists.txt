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

include(Coroutines)
include(JoltTests)
enable_coroutines()

file(GLOB PROCESSOR_UNIT_TESTS  "unit/*.cpp")
file(GLOB PROCESSOR_INTEGRATION_TESTS "integration/*.cpp")
file(GLOB RESOURCE_APPS "resource_apps/*.cpp")

if (WIN32)
    list(REMOVE_ITEM PROCESSOR_UNIT_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/ExecuteProcessTests.cpp")
endif()
SET(PROCESSOR_INT_TEST_COUNT 0)

FOREACH(testfile ${PROCESSOR_UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE ${PROCESSOR_INCLUDE_DIRS})
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} BEFORE PRIVATE "../")
    target_include_directories(${testfilename} BEFORE PRIVATE "../processors")
    target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    target_link_libraries(${testfilename} minifi-expression-language-extensions)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} minifi-civet-extensions)
    target_compile_definitions("${testfilename}" PRIVATE JOLT_TESTS_DIR="${jolt_tests_SOURCE_DIR}/jolt-core/src/test/resources/json/shiftr")
    if (NOT DISABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    MATH(EXPR PROCESSOR_INT_TEST_COUNT "${PROCESSOR_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${PROCESSOR_INT_TEST_COUNT} processor unit test file(s)...")


SET(INT_TEST_COUNT 0)
FOREACH(testfile ${PROCESSOR_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE ${PROCESSOR_INCLUDE_DIRS})
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} BEFORE PRIVATE "../")
    target_include_directories(${testfilename} BEFORE PRIVATE "../processors")
    target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    createTests("${testfilename}")
    target_link_libraries(${testfilename})
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} minifi-civet-extensions)
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    if (NOT DISABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()

    MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()
message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

add_test(NAME TailFileTest COMMAND TailFileTest "${TEST_RESOURCES}/TestTailFile.yml"  "${TEST_RESOURCES}/")
add_test(NAME TailFileCronTest COMMAND TailFileTest "${TEST_RESOURCES}/TestTailFileCron.yml"  "${TEST_RESOURCES}/")
add_test(NAME ProcessGroupTest COMMAND ProcessGroupTest "${TEST_RESOURCES}/TestProcessGroup.yml")

FOREACH(resourcefile ${RESOURCE_APPS})
    get_filename_component(resourcefilename "${resourcefile}" NAME_WE)
    add_minifi_executable("${resourcefilename}" "${resourcefile}")
    set_target_properties(${resourcefilename} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
ENDFOREACH()

copyTestResources(${CMAKE_SOURCE_DIR}/libminifi/test/resources/certs ${CMAKE_BINARY_DIR}/bin/resources)
