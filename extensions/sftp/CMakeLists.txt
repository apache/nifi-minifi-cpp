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

find_package(CURL REQUIRED)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(client processors)

file(GLOB SOURCES  "*.cpp" "client/*.cpp" "processors/*.cpp")

if (USE_CURL_NSS)
    message("Using NSS")
	add_definitions(-DUSE_CURL_NSS)
endif()

add_library(minifi-sftp STATIC ${SOURCES})
set_property(TARGET minifi-sftp PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
	target_compile_options(PUBLIC minifi-sftp "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
	target_link_libraries(minifi-sftp "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (CURL_FOUND)
	include_directories(${CURL_INCLUDE_DIRS})
	if (WIN32)
		message("Including ${CURL_LIBRARY}")
		target_link_libraries (minifi-sftp ${CURL_LIBRARY})
		set_target_properties(minifi-sftp PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:${CURL_LIBRARY}")
	else()
		target_link_libraries(minifi-sftp ${CURL_LIBRARIES})
	endif()
endif ()

# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-sftp ${LIBMINIFI} ${UUID_LIBRARIES})

# Include OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

target_link_libraries(minifi-sftp ${CMAKE_DL_LIBS})

# Include LibSSH2
find_package(LibSSH2 REQUIRED)
include_directories(${LIBSSH2_INCLUDE_DIR})
target_link_libraries(minifi-sftp ${LIBSSH2_LIBRARY})

# Include zlib
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(minifi-sftp ${ZLIB_LIBRARIES})

# Include RapidJSON
include_directories(thirdparty/rapidjson-1.1.0/include)

if (WIN32)
message("${OPENSSL_LIBRARIES}")
	set (WIN32_ARCHIVES "")
		foreach(OPENSSL_LIB ${OPENSSL_LIBRARIES})
		if (WIN32)
		if (OPENSSL_LIB MATCHES "\\.lib$" OR OPENSSL_LIB MATCHES "\\.dll$" )
				message( FATAL "Including ${OPENSSL_LIB}")
		set_target_properties(minifi-sftp PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:${OPENSSL_LIB}")
		endif()
	endif()
	endforeach()

elseif (APPLE)
	target_link_libraries(minifi-sftp ${OPENSSL_LIBRARIES})
    set_target_properties(minifi-sftp PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
	message("${OPENSSL_LIBRARIES}")
	foreach(OPENSSL_LIB ${OPENSSL_LIBRARIES})
		target_link_libraries (minifi-sftp ${OPENSSL_LIB})
	endforeach()
endif ()

SET (SFTP minifi-sftp PARENT_SCOPE)
register_extension(minifi-sftp)
