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


file(GLOB EXPRESSION_LANGUAGE_TESTS  "*.cpp")

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXPRESSION_LANGUAGE_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/expression-language")
	createTests("${testfilename}")
	target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
	if (APPLE)
		target_link_libraries ("${testfilename}" -Wl,-all_load minifi-expression-language-extensions )
	else ()
		target_link_libraries ("${testfilename}" -Wl,--whole-archive minifi-expression-language-extensions -Wl,--no-whole-archive)
	endif()
	MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()


### integration tests

file(GLOB INT_EXPRESSION_LANGUAGE_TESTS  "integration/*.cpp")

SET(INT_EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${INT_EXPRESSION_LANGUAGE_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/expression-language")
	createTests("${testfilename}")
	if (APPLE)
		target_link_libraries ("${testfilename}" -Wl,-all_load minifi-expression-language-extensions )
	else ()
		target_link_libraries ("${testfilename}" -Wl,--whole-archive minifi-expression-language-extensions -Wl,--no-whole-archive)
	endif()
	MATH(EXPR EXTENSIONS_TEST_COUNT "${INT_EXTENSIONS_TEST_COUNT}+1")
ENDFOREACH()


add_test(NAME UpdateAttributeIntegrationTest COMMAND UpdateAttributeIntegrationTest "${TEST_RESOURCES}/TestUpdateAttribute.yml"  "${TEST_RESOURCES}/")

message("-- Finished building ${EXTENSIONS_TEST_COUNT} expression language related test file(s)...")
