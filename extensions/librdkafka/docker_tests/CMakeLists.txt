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

file(GLOB KAFKA_TESTS  "*.cpp")

SET(KAFKA_TEST_COUNT 0)

FOREACH(testfile ${KAFKA_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/librdkafka/")
    target_link_libraries(${testfilename} minifi-rdkafka-extensions)
    createTests("${testfilename}")
    MATH(EXPR KAFKA_TEST_COUNT "${KAFKA_TEST_COUNT}+1")
    # The line below handles integration test
    add_test(NAME "${testfilename}" COMMAND "${testfilename}")
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
ENDFOREACH()

message("-- Finished building ${KAFKA_TEST_COUNT} Kafka related test file(s)...")
