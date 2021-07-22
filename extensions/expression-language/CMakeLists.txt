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
if(WIN32)
	include(FetchContent)

	set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

	FetchContent_Declare(
		winflexbison
		GIT_REPOSITORY "https://github.com/lexxmark/winflexbison.git"
		GIT_TAG "a72f3d6b5102b65f064a5054ba634d3d62e94f41"
	)
	FetchContent_GetProperties("winflexbison")

	if(NOT winflexbison_POPULATED)
		FetchContent_Populate("winflexbison")
		execute_process(
			COMMAND ${CMAKE_COMMAND} .
			WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
			RESULT_VARIABLE result
			OUTPUT_VARIABLE bisonbuild
			ERROR_VARIABLE bisonbuildE
			)

		execute_process(
			COMMAND ${CMAKE_COMMAND} --build . --config RelWithDebInfo
			WORKING_DIRECTORY ${winflexbison_SOURCE_DIR}
			RESULT_VARIABLE result
			OUTPUT_VARIABLE bisonbuild
			ERROR_VARIABLE bisonbuildE
			)

		file(COPY ${winflexbison_SOURCE_DIR}/bison/Data DESTINATION ${winflexbison_SOURCE_DIR}/bison/RelWithDebInfo/)
	endif()

	set(BISON_EXECUTABLE "${winflexbison_SOURCE_DIR}/bison/RelWithDebInfo/win_bison.exe" CACHE PATH "bison executable")
	set(FLEX_EXECUTABLE "${winflexbison_SOURCE_DIR}/flex/RelWithDebInfo/win_flex.exe" CACHE PATH "flex executable")

	include_directories(${winflexbison_SOURCE_DIR}/flex/src/)

	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/winflexbison")
endif()

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

bison_target(
	el-parser
	${CMAKE_CURRENT_SOURCE_DIR}/Parser.yy
	${CMAKE_CURRENT_SOURCE_DIR}/Parser.cpp
)

flex_target(
	el-scanner
	${CMAKE_CURRENT_SOURCE_DIR}/Scanner.ll
	${CMAKE_CURRENT_SOURCE_DIR}/Scanner.cpp
    COMPILE_FLAGS --c++
)

add_flex_bison_dependency(el-scanner el-parser)

include_directories(./ ../../libminifi/include  ../../libminifi/include/core)
include_directories(SYSTEM ../../thirdparty/)
include_directories(common)
include_directories(impl)

if(WIN32)
	include_directories(../../libminifi/opsys/win)
	set(SOCKET_SOURCES "src/io/win/*.cpp")
else()
	include_directories(../../libminifi/opsys/posix)
	set(SOCKET_SOURCES "src/io/posix/*.cpp")
endif()


file(GLOB SOURCES  "*.cpp")

if (NOT WIN32)
	set_source_files_properties(${BISON_el-parser_OUTPUTS} PROPERTIES COMPILE_FLAGS -Wno-error)
	set_source_files_properties(${FLEX_el-scanner_OUTPUTS} PROPERTIES COMPILE_FLAGS -Wno-error)
endif()

add_library(minifi-expression-language-extensions SHARED ${SOURCES} ${BISON_el-parser_OUTPUTS} ${FLEX_el-scanner_OUTPUTS})

target_link_libraries(minifi-expression-language-extensions ${LIBMINIFI})
target_link_libraries(minifi-expression-language-extensions date::tz RapidJSON CURL::libcurl)

SET (EXPRESSION-LANGUAGE-EXTENSIONS minifi-expression-language-extensions PARENT_SCOPE)

register_extension(minifi-expression-language-extensions)
register_extension_linter(minifi-expression-language-extensions-linter)
