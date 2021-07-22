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

set(WEL_INTEGRATION_TESTS  "BookmarkTests.cpp" "ConsumeWindowsEventLogTests.cpp" "MetadataWalkerTests.cpp")
if (TEST_CUSTOM_WEL_PROVIDER)
	execute_process(COMMAND
		"${CMAKE_CURRENT_LIST_DIR}/custom-provider/generate-and-register.bat"
		"${CMAKE_CURRENT_LIST_DIR}/custom-provider"
	)
	list(APPEND WEL_INTEGRATION_TESTS "CWELCustomProviderTests.cpp")
endif()

SET(WEL_TEST_COUNT 0)
FOREACH(testfile ${WEL_INTEGRATION_TESTS})
  	get_filename_component(testfilename "${testfile}" NAME_WE)
  	add_executable("${testfilename}" "${testfile}")
  	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/windows-event-log/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/include/")
	createTests("${testfilename}")
	target_link_libraries(${testfilename} minifi-wel)
	target_link_libraries(${testfilename} minifi-standard-processors)
	target_link_libraries (${testfilename} ${LIBMINIFI} ${CATCH_MAIN_LIB})

	MATH(EXPR WEL_TEST_COUNT "${WEL_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
ENDFOREACH()
message("-- Finished building ${WEL_TEST_COUNT} WEL related test file(s)...")
