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

include(Catch2)

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

set(NANOFI_TEST_DIR "${CMAKE_SOURCE_DIR}/nanofi/tests/")

function(copyTestResources SOURCE_DIR DEST_DIR)
    file(GLOB_RECURSE RESOURCE_FILES "${SOURCE_DIR}/*")
    foreach(RESOURCE_FILE ${RESOURCE_FILES})
        if(IS_DIRECTORY "${RESOURCE_FILE}")
            continue()
        endif()

        file(RELATIVE_PATH RESOURCE_RELATIVE_PATH "${SOURCE_DIR}" "${RESOURCE_FILE}")
        get_filename_component(RESOURCE_DEST_DIR "${DEST_DIR}/${RESOURCE_RELATIVE_PATH}" DIRECTORY)

        file(MAKE_DIRECTORY "${RESOURCE_DEST_DIR}")
        set(dest_file "${DEST_DIR}/${RESOURCE_RELATIVE_PATH}")

        configure_file(${RESOURCE_FILE} ${dest_file} COPYONLY)
    endforeach()
endfunction()

function(appendIncludes testName)
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
    if(WIN32)
        target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/win")
        target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/win/io")
    else()
        target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/posix")
        target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/posix/io")
    endif()
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/utils")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/processors")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/provenance")
endfunction()

function(createTests testName)
    message(DEBUG "-- Adding test: ${testName}")
    appendIncludes("${testName}")

    target_link_libraries(${testName} ${CMAKE_DL_LIBS})
    target_wholearchive_library(${testName} ${TEST_BASE_LIB})
    target_link_libraries(${testName} core-minifi yaml-cpp spdlog Threads::Threads)
    target_compile_definitions(${testName} PRIVATE LOAD_EXTENSIONS)
    set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

enable_testing(test)

SET(TEST_BASE_LIB test_base)
set(TEST_BASE_SOURCES "TestBase.cpp" "StatefulProcessor.cpp" "WriteToFlowFileTestProcessor.cpp" "ReadFromFlowFileTestProcessor.cpp" "DummyProcessor.cpp")
list(TRANSFORM TEST_BASE_SOURCES PREPEND "${TEST_DIR}/")
add_minifi_library(${TEST_BASE_LIB} STATIC ${TEST_BASE_SOURCES})
target_link_libraries(${TEST_BASE_LIB} core-minifi)
target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
if(WIN32)
    target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/win")
else()
    target_include_directories(${TEST_BASE_LIB} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/opsys/posix")
endif()

file(COPY ${TEST_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})
SET(TEST_RESOURCES ${CMAKE_BINARY_DIR}/resources)

GETSOURCEFILES(UNIT_TESTS "${TEST_DIR}/unit/")
GETSOURCEFILES(TLS_UNIT_TESTS "${TEST_DIR}/unit/tls/")
GETSOURCEFILES(NANOFI_UNIT_TESTS "${NANOFI_TEST_DIR}")
GETSOURCEFILES(INTEGRATION_TESTS "${TEST_DIR}/integration/")

if (NOT WIN32)
    list(REMOVE_ITEM UNIT_TESTS WindowsCertStoreLocationTests.cpp)
endif()

SET(UNIT_TEST_COUNT 0)
FOREACH(testfile ${UNIT_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/${testfile}")
    target_compile_definitions("${testfilename}" PRIVATE TZ_DATA_DIR="${CMAKE_BINARY_DIR}/tzdata")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} Catch2WithMain)
    MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
ENDFOREACH()
message("-- Finished building ${UNIT_TEST_COUNT} unit test file(s)...")

if (MINIFI_OPENSSL)
    SET(UNIT_TEST_COUNT 0)
    FOREACH(testfile ${TLS_UNIT_TESTS})
        get_filename_component(testfilename "${testfile}" NAME_WE)
        add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/tls/${testfile}")
        createTests("${testfilename}")
        MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
        add_test(NAME "${testfilename}" COMMAND "${testfilename}" "${TEST_RESOURCES}/")
    ENDFOREACH()
    message("-- Finished building ${UNIT_TEST_COUNT} TLS unit test file(s)...")
endif()

if(NOT WIN32 AND ENABLE_NANOFI)
    SET(UNIT_TEST_COUNT 0)
    FOREACH(testfile ${NANOFI_UNIT_TESTS})
        get_filename_component(testfilename "${testfile}" NAME_WE)
        add_minifi_executable("${testfilename}" "${NANOFI_TEST_DIR}/${testfile}")
        target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/nanofi/include")
        target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
        target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/processors/")
        target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test")
        appendIncludes("${testfilename}")
        target_link_libraries(${testfilename} Catch2WithMain Threads::Threads)

        target_wholearchive_library(${testfilename} nanofi)

        createTests(${testfilename})

        target_link_libraries(${testfilename} minifi-standard-processors)

        MATH(EXPR UNIT_TEST_COUNT "${UNIT_TEST_COUNT}+1")
        add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    ENDFOREACH()
    message("-- Finished building ${UNIT_TEST_COUNT} NanoFi unit test file(s)...")
endif()

SET(INT_TEST_COUNT 0)
FOREACH(testfile ${INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/integration/${testfile}")
    createTests("${testfilename}")
    MATH(EXPR INT_TEST_COUNT "${INT_TEST_COUNT}+1")
ENDFOREACH()

target_link_libraries(OnScheduleErrorHandlingTests minifi-test-processors)
target_wholearchive_library(StateTransactionalityTests minifi-standard-processors)

add_test(NAME OnScheduleErrorHandlingTests COMMAND OnScheduleErrorHandlingTests "${TEST_RESOURCES}/TestOnScheduleRetry.yml"  "${TEST_RESOURCES}/")
add_test(NAME StateTransactionalityTests COMMAND StateTransactionalityTests "${TEST_RESOURCES}/TestStateTransactionality.yml")

message("-- Finished building ${INT_TEST_COUNT} integration test file(s)...")

get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
foreach(EXTENSION ${extensions})
    add_subdirectory(${EXTENSION})
endforeach()
