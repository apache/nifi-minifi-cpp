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

file(GLOB SOURCES  "*.cpp")

add_library(sftp-test-tools STATIC ${SOURCES})

target_include_directories(sftp-test-tools BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include")
target_include_directories(sftp-test-tools BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(sftp-test-tools BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
target_include_directories(sftp-test-tools BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/core")
target_include_directories(sftp-test-tools BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/io")

target_link_libraries(sftp-test-tools core-minifi)

set(SFTP_TEST_SERVER_JAR_SOURCE "${CMAKE_SOURCE_DIR}/extensions/sftp/tests/tools/sftp-test-server")
set(SFTP_TEST_SERVER_JAR_BIN "${CMAKE_BINARY_DIR}/bin/sftp-test/tools" )

file(COPY ${SFTP_TEST_SERVER_JAR_SOURCE} DESTINATION ${SFTP_TEST_SERVER_JAR_BIN})

message("Building SFTPTestServer ....")
# duplicating the execute_process because a version of cmake tested exhibited failures
# with an empty cmd before ${MAVEN_EXECUTABLE}. Until this is resolved ( or proven to be a non cmake issue on all versions)
# we should stick with the the minor duplication
if (WIN32)
    execute_process(COMMAND cmd /c "${MAVEN_EXECUTABLE}" "-q" "package" "-Denforcer.skip=true"
            WORKING_DIRECTORY "${SFTP_TEST_SERVER_JAR_BIN}/sftp-test-server"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
else()
    execute_process(COMMAND "${MAVEN_EXECUTABLE}" "-q" "package" "-Denforcer.skip=true"
            WORKING_DIRECTORY "${SFTP_TEST_SERVER_JAR_BIN}/sftp-test-server"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
endif()
if("${mvn_result}" STREQUAL "0")
    message("Successfully built SFTPTestServer")
    message("${mvn_output}")
else()
    message("Maven failed ${mvn_result} ${mvn_output} ${mvn_error}")
endif()
