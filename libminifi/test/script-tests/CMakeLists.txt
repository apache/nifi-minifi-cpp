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

if (NOT DISABLE_PYTHON_SCRIPTING)
	file(GLOB EXECUTESCRIPT_PYTHON_INTEGRATION_TESTS  "Python*.cpp")
endif()

if (ENABLE_LUA_SCRIPTING)
	file(GLOB EXECUTESCRIPT_LUA_INTEGRATION_TESTS  "Lua*.cpp")
endif()

SET(EXTENSIONS_TEST_COUNT 0)

FOREACH(testfile ${EXECUTESCRIPT_PYTHON_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script/python")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/pybind11/include")
	add_definitions(-DPYTHON_SUPPORT)
	if (APPLE)
		target_link_libraries ("${testfilename}" -Wl,-all_load ${ZLIB_LIBRARIES} minifi-script-extensions )
	else ()
		target_link_libraries ("${testfilename}" -Wl,--whole-archive ${ZLIB_LIBRARIES}  minifi-script-extensions -Wl,--no-whole-archive)
	endif()
	createTests("${testfilename}")
	MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()

FOREACH(testfile ${EXECUTESCRIPT_LUA_INTEGRATION_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/script/lua")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/thirdparty/sol2-2.17.5")
	add_definitions(-DLUA_SUPPORT)
	if (APPLE)
		target_link_libraries ("${testfilename}"  -Wl,-all_load ${ZLIB_LIBRARIES} minifi-script-extensions )
	else ()
		target_link_libraries ("${testfilename}" -Wl,--whole-archive ${ZLIB_LIBRARIES}  minifi-script-extensions -Wl,--no-whole-archive)
	endif()
	createTests("${testfilename}")
	MATH(EXPR EXTENSIONS_TEST_COUNT "${EXTENSIONS_TEST_COUNT}+1")
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()

message("-- Finished building ${PYTHON_EXECUTESCRIPT-EXTENSIONS_TEST_COUNT} Python ExecuteScript related test file(s)...")
