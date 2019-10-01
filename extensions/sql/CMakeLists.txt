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

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(../../thirdparty/rapidjson-1.1.0/include/ ../../thirdparty/rapidjson-1.1.0/include/rapidjson)
set(EXTRA_SOURCES "")
if (WIN32)
#  set(EXTRA_SOURCES "win/*.cpp")
endif()

file(GLOB SOURCES  "*.cpp" "services/*.cpp" "processors/*.cpp"  "data/*.cpp" ${EXTRA_SOURCES} )

add_library(minifi-sql STATIC ${SOURCES})
set_property(TARGET minifi-sql PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-sql "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-sql "${CMAKE_THREAD_LIBS_INIT}")
endif()

find_package(Patch)

if(NOT WIN32)
set(ODBC_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-install/" CACHE STRING "iODBC install directory")
	
  ExternalProject_Add(
    iodbc-external
    GIT_REPOSITORY "https://github.com/openlink/iODBC.git"
    GIT_TAG "d9293505804e775488582321420e427464e6c3ce" 
    BUILD_IN_SOURCE true
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/iodbc-src"
 	BUILD_COMMAND make
    CMAKE_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND make install
    CONFIGURE_COMMAND ""
    PATCH_COMMAND ./autogen.sh && ./configure --prefix=${ODBC_BYPRODUCT_DIR}
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
    BUILD_BYPRODUCTS ${ODBC_BYPRODUCT}
  )

  endif()
set(SOCI_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/soci-install" CACHE STRING "SOCI install directory")
if(WIN32)
	set(BYPRODUCT "${SOCI_BYPRODUCT_DIR}/lib/libsoci_core_4_0.lib")
	set(ODBC_BYPRODUCT "${SOCI_BYPRODUCT_DIR}/lib/libsoci_odbc_4_0.lib")
	set(ODBC_OPTION "")
	set(CMAKE_MODULE "")
else()
	set(BYPRODUCT "${SOCI_BYPRODUCT_DIR}/lib64/libsoci_core.a")
	set(ODBC_OPTION "-DODBC_BYPRODUCT_DIR=${ODBC_BYPRODUCT_DIR}")
	set(ODBC_BYPRODUCT "${SOCI_BYPRODUCT_DIR}/lib64/libsoci_odbc.a")
	set(CMAKE_MODULE "-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
endif()



  ExternalProject_Add(
    sql-external
    GIT_REPOSITORY "https://github.com/SOCI/soci.git"
    GIT_TAG "04e1870294918d20761736743bb6136314c42dd5" 
    BUILD_IN_SOURCE 1
    PATCH_COMMAND ${Patch_EXECUTABLE} -f ${CMAKE_CURRENT_BINARY_DIR}/thirdparty/soci-src/CMakeLists.txt ${CMAKE_CURRENT_SOURCE_DIR}/patch/soci.patch
	SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/soci-src"
 	CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
			   "-DSOCI_TESTS=OFF"
			   ${CMAKE_MODULE}
			   "-DSOCI_SHARED=OFF"
			   "-DSOCI_CXX_C11=ON"
			   "-DWITH_ODBC=ON"
			   "-DSOCI_ODBC=ON"
			   "-DWITH_BOOST=OFF"
			   "-DCMAKE_BUILD_TYPE=Release"
			   ${ODBC_OPTION}
               "-DCMAKE_INSTALL_PREFIX=${SOCI_BYPRODUCT_DIR}"
    BUILD_BYPRODUCTS ${BYPRODUCT}
  )

if(NOT WIN32)  
	add_dependencies(sql-external iodbc-external)
endif()  

include_directories("./")

include_directories("${SOCI_BYPRODUCT_DIR}/include/")
include_directories("${SOCI_BYPRODUCT_DIR}/include/soci")
add_dependencies(minifi-sql sql-external)
target_link_libraries(minifi-sql ${LIBMINIFI})


if (NOT WIN32)
	if (APPLE)
		target_link_libraries (minifi-sql  -Wl,-all_load "${ODBC_BYPRODUCT_DIR}/lib/libodbc.a")
		target_link_libraries (minifi-sql  -Wl,-all_load ${BYPRODUCT})
		target_link_libraries (minifi-sql  -Wl,-all_load ${ODBC_BYPRODUCT})
	else()
		target_link_libraries (minifi-sql  "-Wl,--whole-archive ${ODBC_BYPRODUCT_DIR}/lib/libodbc.a -Wl,--no-whole-archive")
		target_link_libraries (minifi-sql  "-Wl,--whole-archive ${ODBC_BYPRODUCT} -Wl,--no-whole-archive")
		target_link_libraries (minifi-sql  "-Wl,--whole-archive ${BYPRODUCT} -Wl,--no-whole-archive")
		message("Linking whole archives")
	endif()
else()
	find_package(ODBC REQUIRED)
	target_link_libraries (minifi-sql  ${BYPRODUCT} ${ODBC_BYPRODUCT} ${ODBC_LIBRARIES} )
endif()

if (WIN32)
    set_target_properties(minifi-sql PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE:${BYPRODUCT}"
    )
	set_target_properties(minifi-sql PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE:${ODBC_BYPRODUCT}"
    )
elseif (APPLE)
    set_target_properties(minifi-sql PROPERTIES
        LINK_FLAGS "-Wl,-all_load  ${ODBC_BYPRODUCT_DIR}/lib/libodbc.a "
    )
else ()
    set_target_properties(minifi-sql PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (SQL-EXTENSION minifi-sql PARENT_SCOPE)

register_extension(minifi-sql)


register_extension_linter(minifi-sql-linter)