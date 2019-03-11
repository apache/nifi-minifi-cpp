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

cmake_minimum_required(VERSION 2.6)

IF(POLICY CMP0048)
  CMAKE_POLICY(SET CMP0048 OLD)
ENDIF(POLICY CMP0048)

include_directories(../libminifi/include ../thirdparty/spdlog-20170710/include ../thirdparty/concurrentqueue ../thirdparty/yaml-cpp-yaml-cpp-20171024/include ../thirdparty/rapidjson-1.1.0/include ../thirdparty/)
include_directories(${JEMALLOC_INCLUDE_DIRS})

if(WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	include_directories(../libminifi/opsys/win)
else()
	include_directories(../libminifi/opsys/posix)
endif()


include(CheckCXXCompilerFlag)
if (WIN32)
  if ((MSVC_VERSION GREATER "1900") OR (MSVC_VERSION EQUAL "1900"))
	    CHECK_CXX_COMPILER_FLAG("/std:c++14" _cpp_latest_flag_supported)
	    if (_cpp_latest_flag_supported)
	        add_compile_options("/std:c++14")
	    endif()
	endif()
else()
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
 message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

endif()

add_executable(minifiexe MiNiFiMain.cpp)
#if(THREADS_HAVE_PTHREAD_ARG)
#  target_compile_options(PUBLIC minifiexe "-pthread")
#endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifiexe "${CMAKE_THREAD_LIBS_INIT}")
endif()

# Include UUID

set (WIN32_ARCHIVES "")



# Link against minifi, yaml-cpp, civetweb-cpp, uuid, openssl, and rocksdb
#target_link_libraries(minifiexe core-minifi)

if (APPLE)
	target_link_libraries (minifiexe -Wl,-all_load core-minifi)
elseif(NOT WIN32)
	target_link_libraries (minifiexe -Wl,--whole-archive core-minifi -Wl,--no-whole-archive)
else()
	#target_link_libraries (minifiexe core-minifi)
	set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:core-minifi")
#	set_target_properties(minifiexe PROPERTIES LINK_FLAGS "/WHOLEARCHIVE:core-minifi")
endif ()

# Include OpenSSL
if (OPENSSL_FOUND)
	if (NOT USE_SYSTEM_OPENSSL)
		if (APPLE)
			target_link_libraries(minifiexe -Wl,-all_load ${OPENSSL_LIBRARIES})
		elseif(NOT WIN32)
			target_link_libraries(minifiexe -Wl,--whole-archive ${OPENSSL_LIBRARIES} -Wl,--no-whole-archive)
		else()
		target_link_libraries(minifiexe ${OPENSSL_LIBRARIES})
		endif()
	else()
		target_link_libraries(minifiexe ${OPENSSL_LIBRARIES})
	endif()
	include_directories(${OPENSSL_INCLUDE_DIR})
endif(OPENSSL_FOUND)

add_dependencies(minifiexe minifi)


if (APPLE)
	target_link_libraries (minifiexe -Wl,-all_load minifi)
elseif(NOT WIN32)
	target_link_libraries (minifiexe -Wl,--whole-archive minifi -Wl,--no-whole-archive)
else()
	target_link_libraries (minifiexe minifi)
	set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:minifi")
	#set_target_properties(minifiexe PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:minifi")
endif ()

target_link_libraries(minifiexe yaml-cpp ${UUID_LIBRARIES} ${JEMALLOC_LIBRARIES}) #

if (WIN32)
	include_directories("../thirdparty/Simple-Windows-Posix-Semaphore")
  	target_link_libraries(minifiexe semaphore)
endif()



if (APPLE)
	get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
	foreach(EXTENSION ${extensions})
		message("Linking MiNiFiMain against ${EXTENSION}")
		target_link_libraries (minifiexe -Wl,-all_load ${EXTENSION})
		add_dependencies(minifiexe ${EXTENSION})
	endforeach()    
else ()
	get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
	foreach(EXTENSION ${extensions})
	if (WIN32)
	target_link_libraries (minifiexe ${EXTENSION})
	  set(WIN32_ARCHIVES "${WIN32_ARCHIVES} /WHOLEARCHIVE:${EXTENSION}")
	  add_dependencies(minifiexe ${EXTENSION})
    else()
	  target_link_libraries (minifiexe -Wl,--whole-archive ${EXTENSION} -Wl,--no-whole-archive)
	  add_dependencies(minifiexe ${EXTENSION})
	  endif()
	endforeach()
endif ()

if(WIN32)
	set_target_properties(minifiexe PROPERTIES LINK_FLAGS "${LINK_FLAGS} ${WIN32_ARCHIVES}")
endif()
set_target_properties(minifiexe
        PROPERTIES OUTPUT_NAME minifi)

               

if (NOT WIN32)
add_custom_command(TARGET minifiexe POST_BUILD
           COMMAND cat ${CMAKE_BINARY_DIR}/all.log)
else()
#add_custom_command(TARGET minifiexe POST_BUILD
#           COMMAND type ${CMAKE_BINARY_DIR}/all.log)
endif()
