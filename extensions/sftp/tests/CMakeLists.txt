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

# Java is required for SFTPTestServer, we will disable tests if we can't find it
message(STATUS "JAVA_HOME: '$ENV{JAVA_HOME}'")
find_package(Java)
find_package(Maven)
message(STATUS "MAVEN: ${MAVEN_EXECUTABLE}")

if (NOT SKIP_TESTS AND Java_FOUND AND Maven_FOUND AND NOT DISABLE_EXPRESSION_LANGUAGE)
	file(GLOB SFTP_INTEGRATION_TESTS "*.cpp")
	SET(SFTP-EXTENSIONS_TEST_COUNT 0)
	FOREACH(testfile ${SFTP_INTEGRATION_TESTS})
		get_filename_component(testfilename "${testfile}" NAME_WE)
		add_executable("${testfilename}" "${testfile}")
		target_include_directories(${testfilename} BEFORE PRIVATE ${LIBSSH2_INCLUDE_DIR})
		target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
		target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/processors")
		target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
		target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/nanofi/include")
		target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/sftp")
		target_include_directories(${testfilename} BEFORE PRIVATE "../")
		target_include_directories(${testfilename} BEFORE PRIVATE "../client/")
		target_include_directories(${testfilename} BEFORE PRIVATE "../processors/")
		target_include_directories(${testfilename} BEFORE PRIVATE ./include)
		target_include_directories(${testfilename} BEFORE PRIVATE ./tools)

		createTests("${testfilename}")
		target_link_libraries(${testfilename} ${CATCH_MAIN_LIB} core-minifi sftp-test-tools)
		target_link_libraries(${testfilename} minifi-sftp)
		target_link_libraries(${testfilename} minifi-expression-language-extensions)
		target_link_libraries(${testfilename} minifi-standard-processors)
		if (NOT DISABLE_ROCKSDB)
			target_link_libraries(${testfilename} minifi-rocksdb-repos)
		endif()

		MATH(EXPR SFTP-EXTENSIONS_TEST_COUNT "${SFTP-EXTENSIONS_TEST_COUNT}+1")
		add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
	ENDFOREACH()
	message("-- Finished building ${SFTP-EXTENSIONS_TEST_COUNT} SFTP related test file(s)...")

	# Copy test resources
	file(COPY "${CMAKE_SOURCE_DIR}/extensions/sftp/tests/resources" DESTINATION "${CMAKE_BINARY_DIR}/bin/sftp-test/")

	add_subdirectory(tools)
else()
	message("Could find Java and Maven to build SFTPTestServer, disabling SFTP tests")
endif()
