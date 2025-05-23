# Licensed to the Apache Software Foundation (ASF) under one
#
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

if (NOT MINIFI_PERFORMANCE_TESTS)
    return()
endif()

include(FetchBenchmark)

GETSOURCEFILES(PERF_TESTS "${TEST_DIR}/unit/performance")

SET(PERF_TEST_COUNT 0)
FOREACH(testfile ${PERF_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${TEST_DIR}/unit/performance/${testfile}")
    target_link_libraries(${testfilename} benchmark::benchmark core-minifi)
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
    MATH(EXPR PERF_TEST_COUNT "${PERF_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    set_tests_properties(${testfilename} PROPERTIES LABELS "performance")
ENDFOREACH()
message("-- Finished building ${PERF_TEST_COUNT} performance test file(s)...")
