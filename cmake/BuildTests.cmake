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

include(GetCatch2)
get_catch2()

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
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core/parameter-providers")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/io")
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/expression-language")
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
    target_wholearchive_library(${testName} libminifi-unittest)
    target_link_libraries(${testName} core-minifi yaml-cpp spdlog Threads::Threads)
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/libtest/")
    target_compile_definitions(${testName} PRIVATE LOAD_EXTENSIONS)
    set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

function(createIntegrationTests testName)
    message(DEBUG "-- Adding integration test: ${testName}")
    appendIncludes("${testName}")

    target_link_libraries(${testName} ${CMAKE_DL_LIBS})
    target_wholearchive_library(${testName} libminifi-integrationtest)
    target_link_libraries(${testName} core-minifi yaml-cpp spdlog Threads::Threads)
    target_include_directories(${testName} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/libtest/")
    target_compile_definitions(${testName} PRIVATE LOAD_EXTENSIONS)
    set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

enable_testing()

file(COPY ${TEST_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})
SET(TEST_RESOURCES ${CMAKE_BINARY_DIR}/resources)

get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
foreach(EXTENSION ${extensions})
    add_subdirectory(${EXTENSION})
endforeach()
