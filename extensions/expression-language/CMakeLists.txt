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
	)

	add_flex_bison_dependency(el-scanner el-parser)

else()


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
)

add_flex_bison_dependency(el-scanner el-parser)
endif()
include_directories(./ ../../libminifi/include  ../../libminifi/include/core ../../thirdparty/cron ../../thirdparty/spdlog-20170710/include ../../thirdparty/concurrentqueue ../../thirdparty/yaml-cpp-yaml-cpp-0.5.3/include ${CIVET_THIRDPARTY_ROOT}/include ../../thirdparty/)
include_directories(common)
include_directories(impl)
include_directories(../../thirdparty/date/include)

if(WIN32)
include_directories(../../libminifi/opsys/win)
set(SOCKET_SOURCES "src/io/win/*.cpp")
else()
include_directories(../../libminifi/opsys/posix)
set(SOCKET_SOURCES "src/io/posix/*.cpp")
endif()


add_subdirectory(../../thirdparty/date ${CMAKE_CURRENT_BINARY_DIR}/date)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-expression-language-extensions STATIC ${SOURCES} ${BISON_el-parser_OUTPUTS} ${FLEX_el-scanner_OUTPUTS})
set_property(TARGET minifi-expression-language-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(minifi-expression-language-extensions minifi tz)

if (NOT DISABLE_CURL)
	if (NOT CURL_FOUND)
		find_package(CURL REQUIRED)
	endif(NOT CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	target_link_libraries(minifi-expression-language-extensions ${CURL_LIBRARIES})
endif()
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(minifi-expression-language-extensions ${CMAKE_DL_LIBS})
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-expression-language-extensions ${ZLIB_LIBRARIES})

if (WIN32 AND NOT DISABLE_CURL)
    set_target_properties(minifi-expression-language-extensions PROPERTIES
        LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:${CURL_LIBRARIES}"
    )
endif()

SET (EXPRESSION-LANGUAGE-EXTENSIONS minifi-expression-language-extensions PARENT_SCOPE)

register_extension(minifi-expression-language-extensions)