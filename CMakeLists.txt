# Licensed to the Apache Software Foundation (ASF) under one
#
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

cmake_minimum_required(VERSION 3.0)
project(nifi-minifi-cpp)
set(PROJECT_NAME "nifi-minifi-cpp")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 7)
set(PROJECT_VERSION_PATCH 0)

include(CMakeDependentOption)
include(CheckIncludeFile)
include(FeatureSummary)
include(ExternalProject)

option(SKIP_TESTS "Skips building all tests." OFF)
option(ENABLE_BENCHMARKING "Enables building of benchmark suites." OFF)

option(PORTABLE "Instructs the compiler to remove architecture specific optimizations" ON)

option(USE_SHARED_LIBS "Builds using shared libraries" ON)

option(ENABLE_PYTHON "Instructs the build system to enable building shared objects for the python lib" OFF)

cmake_dependent_option(STATIC_BUILD "Attempts to statically link as many dependencies as possible." ON "NOT ENABLE_PYTHON; NOT USE_SHARED_LIBS" OFF)

cmake_dependent_option(USE_SYSTEM_OPENSSL "Instructs the build system to search for and use an SSL library available in the host system" ON "NOT STATIC_BUILD" OFF)
option(LIBC_STATIC "Instructs the build system to statically link libstdc++ and glibc into minifiexe. Experiemental" OFF)

option(OPENSSL_OFF "Disables OpenSSL" OFF)
option(ENABLE_OPS "Enable Operations/zlib Tools" ON)
option(USE_SYSTEM_UUID "Instructs the build system to search for and use a UUID library available in the host system" OFF)

option(ENABLE_JNI "Instructs the build system to enable the JNI extension" OFF)
option(ENABLE_OPENCV "Instructs the build system to enable the OpenCV extension" OFF)
cmake_dependent_option(USE_SYSTEM_CURL "Instructs the build system to search for and use a cURL library available in the host system" ON "NOT STATIC_BUILD" OFF)
option(BUILD_SHARED_LIBS "Build yaml cpp shared lib" OFF)

cmake_dependent_option(USE_SYSTEM_ZLIB "Instructs the build system to search for and use a zlib library available in the host system" ON "NOT STATIC_BUILD" OFF)

cmake_dependent_option(USE_SYSTEM_LIBSSH2 "Instructs the build system to search for and use a libssh2 library available in the host system" OFF "NOT STATIC_BUILD" OFF)

option(USE_SYSTEM_BZIP2 "Instructs the build system to search for and use a bzip2 library available in the host system" ON)
option(BUILD_ROCKSDB "Instructs the build system to use RocksDB from the third party directory" ON)
option(FORCE_WINDOWS "Instructs the build system to force Windows builds when WIN32 is specified" OFF)
option(DISABLE_CURL "Disables libCurl Properties." OFF)

option(USE_GOLD_LINKER "Use Gold Linker" OFF)

if(NOT WIN32)
	if (ENABLE_JNI)
	if (NOT DISABLE_JEMALLOC)
	  set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/jemalloc")
	  
	  set(DIR "${BASE_DIR}/extensions/jemalloc-src")
	  set(JE_BYPRODUCT "${DIR}/lib/libjemalloc.a")
	  ExternalProject_Add(
	    jemalloc-external
	    GIT_REPOSITORY "https://github.com/jemalloc/jemalloc.git"
	    GIT_TAG "61efbda7098de6fe64c362d309824864308c36d4" 
	    PREFIX "${BASE_DIR}/extensions/jemalloc"
	    BUILD_IN_SOURCE true
	    SOURCE_DIR "${DIR}"
	    BUILD_COMMAND make
	    CMAKE_COMMAND ""
	    UPDATE_COMMAND ""
	    BUILD_BYPRODUCTS ${JE_BYPRODUCT} 
	    INSTALL_COMMAND ${CMAKE_COMMAND}  -E echo "Skipping install step."
	    CONFIGURE_COMMAND ""
	    PATCH_COMMAND ./autogen.sh && ./configure 
	    STEP_TARGETS build
	    EXCLUDE_FROM_ALL TRUE
	  )
	  
	  add_library(jemalloc STATIC IMPORTED)
	  set_target_properties(jemalloc PROPERTIES IMPORTED_LOCATION "${JE_BYPRODUCT}")
	  add_dependencies(jemalloc jemalloc-external)
	  set(JEMALLOC_FOUND "YES" CACHE STRING "" FORCE)
	  set(JEMALLOC_INCLUDE_DIRS "${DIR}/include" CACHE STRING "" FORCE)
	  set(JEMALLOC_LIBRARIES jemalloc CACHE STRING "" FORCE)
	  set(JEMALLOC_LIBRARY jemalloc CACHE STRING "" FORCE)
	  set(JEMALLOC_LIBRARY jemalloc CACHE STRING "" FORCE)

	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_JNI")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_JNI")

	endif()
endif()

# Use ccache if present
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    message("-- Found ccache: ${CCACHE_FOUND}")
endif(CCACHE_FOUND)

if (UNIX AND USE_GOLD_LINKER AND NOT APPLE )
  execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE ld_version)
  if ("${ld_version}" MATCHES "GNU gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
  endif()
endif()

# check for exec info before we enable the backtrace features.
CHECK_INCLUDE_FILE("execinfo.h" HAS_EXECINFO)
if (ENABLE_OPS AND HAS_EXECINFO AND NOT WIN32)
  add_definitions("-DHAS_EXECINFO=1")
endif()

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
include(CheckCXXCompilerFlag)
if (WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
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

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if (NOT PORTABLE)
  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  endif()
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Search for threads
find_package(Threads REQUIRED)

if (WIN32)
  	add_subdirectory("thirdparty/Simple-Windows-Posix-Semaphore")
endif()

# Provide custom modules for the project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT OPENSSL_OFF)
	if(USE_SYSTEM_OPENSSL)
		# Set the right openssl root path
		if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
		elseif(NOT WIN32)
		set(OPENSSL_ROOT_DIR "/usr/lib/x86_64-linux-gnu")
		else()
			#set(OPENSSL_ROOT_DIR "/usr/lib/x86_64-linux-gnu")
		endif()
	else()
		include(LibreSSL)
		use_libre_ssl(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl")
	endif()	
	
	find_package (OpenSSL REQUIRED)

	if (OPENSSL_FOUND)
	  include_directories(${OPENSSL_INCLUDE_DIR})
	  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENSSL_SUPPORT")
	  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOPENSSL_SUPPORT")
	  MESSAGE("OpenSSL found at ${OPENSSL_LIBRARIES}")
	else ()
	  message( FATAL_ERROR "OpenSSL was not found. Please install OpenSSL" )
	endif (OPENSSL_FOUND)
	
else()
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/winssl")
endif()

if (OPENSSL_ROOT_DIR )
    set(OPENSSL_PASSTHROUGH "-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}")
endif()

set(PASSTHROUGH_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=${ANDROID_PLATFORM}
        -DANDROID_STL=${ANDROID_STL}
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DANDROID_NDK=${ANDROID_NDK}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM}
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        ${OPENSSL_PASSTHROUGH}
        -G${CMAKE_GENERATOR})

include(Compression)

if(WIN32 OR NOT USE_SYSTEM_ZLIB)
	use_bundled_zlib(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	find_package (OpenSSL REQUIRED)
	list(APPEND CMAKE_MODULE_PATH "${SOURCE_DIR}/cmake/zlib/dummy")
endif()

find_package (ZLIB REQUIRED)

if (ZLIB_FOUND)
  include_directories(${ZLIB_INCLUDE_DIRS})
endif()

SET(TEST_DIR ${CMAKE_SOURCE_DIR}/libminifi/test)

include(Extensions)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uuid/dummy")
if (WIN32)
	include_directories("thirdparty/uuid/include/win32/")
else()
	include_directories("thirdparty/uuid/include/posix")
endif()
add_subdirectory("thirdparty/uuid")
include_directories(thirdparty/ut)
set(UUID_FOUND "YES" CACHE STRING "" FORCE)
set(UUID_LIBRARIES "uuid" CACHE STRING "" FORCE)


if (DISABLE_CURL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDISABLE_CURL")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDISABLE_CURL")
endif(DISABLE_CURL)
if(NOT DISABLE_CURL AND (NOT USE_SYSTEM_CURL))
  message("Using bundled cURL")
  
  set(CURL_C_FLAGS "-I${OPENSSL_INCLUDE_DIR}")
  set(CURL_CXX_FLAGS "${CURL_C_FLAGS}")


get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

if ("${LIB64}" STREQUAL "TRUE" AND (NOT WIN32 AND NOT APPLE))
    set(LIBSUFFIX 64)
else()
    set(LIBSUFFIX "")
endif()

  if (WIN32)
 if (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo OR CMAKE_BUILD_TYPE MATCHES Release)
  set(BYPRODUCT "lib/libcurl.lib")
  else()
  set(BYPRODUCT "lib/libcurl-d.lib")
  endif()
  else()
  set(BYPRODUCT "lib${LIBSUFFIX}/libcurl.a")
  endif()

  if (WIN32)
  set (PC "PATCH_COMMAND ./buildconf.bat")
  else()
  endif()

  ExternalProject_Add(
    curl-external
    GIT_REPOSITORY "https://github.com/curl/curl.git"
    GIT_TAG "f3294d9d86e6a7915a967efff2842089b8b0d071"  # Version 7.64.0
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-src"
    CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
               "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install"
                -DBUILD_CURL_EXE=OFF
                -DBUILD_TESTING=OFF
                -DBUILD_SHARED_LIBS=OFF
                -DHTTP_ONLY=ON
                -DLIBRESSL_BIN_DIR=${LIBRESSL_BIN_DIR}
                -DLIBRESSL_SRC_DIR=${LIBRESSL_SRC_DIR}
                -DCURL_DISABLE_CRYPTO_AUTH=ON
                -DCMAKE_USE_LIBSSH2=OFF
                -DHAVE_GLIBC_STRERROR_R=1
                -DHAVE_GLIBC_STRERROR_R__TRYRUN_OUTPUT=""
                -DHAVE_POSIX_STRERROR_R=0
                -DHAVE_POSIX_STRERROR_R__TRYRUN_OUTPUT=""
                -DHAVE_POLL_FINE_EXITCODE=0
                -DHAVE_FSETXATTR_5=0
                -DHAVE_FSETXATTR_5__TRYRUN_OUTPUT=""
               "-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl"
               "-DCMAKE_C_FLAGS=${CURL_C_FLAGS}"
               "-DCMAKE_CXX_FLAGS=${CURL_CXX_FLAGS}"
	${PC}
    BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install/${BYPRODUCT}"
  )

  if(NOT USE_SYSTEM_OPENSSL OR USE_SYSTEM_OPENSSL STREQUAL "OFF")
    add_dependencies(curl-external libressl-portable)
  endif()

  set(CURL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl/" CACHE STRING "" FORCE)
  set(CURL_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install/" CACHE STRING "" FORCE)
  set(CURL_BYPRODUCT_DIR "${BYPRODUCT}" CACHE STRING "" FORCE)


  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/curl/dummy")
  add_library(curl STATIC IMPORTED)
  set_target_properties(curl PROPERTIES IMPORTED_LOCATION "${CURL_BIN_DIR}${BYPRODUCT}")


  if (OPENSSL_FOUND) 
     if (NOT WIN32)
       set_target_properties(curl PROPERTIES INTERFACE_LINK_LIBRARIES ${OPENSSL_LIBRARIES})
	 endif()
  endif(OPENSSL_FOUND)
  add_dependencies(curl curl-external)
  set(CURL_FOUND "YES")
  set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl/include")
  set(CURL_LIBRARY "${CURL_BIN_DIR}${BYPRODUCT}" CACHE STRING "" FORCE)
  set(CURL_LIBRARIES ${CURL_LIBRARY} CACHE STRING "" FORCE)
 else()
  message("Using System cURL")
endif()

if (CURL_FOUND)
  include_directories("${CURL_INCLUDE_DIRS}")
endif()

file(GLOB SPD_SOURCES "thirdparty/spdlog-20170710/include/spdlog/*")

include(ExternalProject)

set(CIVETWEB_ENABLE_SSL_DYNAMIC_LOADING OFF CACHE BOOL "Disable dynamic SSL library loading")
set(CIVETWEB_ENABLE_CXX ON CACHE BOOL "Enable civet C++ library")
set(CIVETWEB_ENABLE_SSL OFF CACHE BOOL "DISABLE SSL")

SET(WITH_TOOLS OFF CACHE BOOL "Do not build RocksDB tools")
if ( NOT APPLE)
if (ENABLE_PYTHON)
  SET(BUILD_SHARED_LIBS ON CACHE BOOL "Build yaml cpp shared lib" FORCE)
else()
  SET(BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml cpp shared lib" FORCE)
endif()
endif()
SET(WITH_TESTS OFF CACHE BOOL "Build RocksDB library (not repo) tests")
set(CIVET_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/civetweb-1.10/" CACHE STRING "Path to CivetWeb root")
set(CIVET_BINARY_ROOT "${CMAKE_BINARY_DIR}/thirdparty/civetweb-1.10/" CACHE STRING "Path to CivetWeb binary output")
set(ROCKSDB_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/rocksdb/" CACHE STRING "Path to RocksDB root")
add_subdirectory(thirdparty/yaml-cpp-yaml-cpp-20171024)

include_directories(thirdparty/concurrentqueue)
include_directories(thirdparty/yaml-cpp-yaml-cpp-20171024/include)
include_directories(thirdparty/rapidjson-1.1.0/include)
include_directories(thirdparty/mio/include)

## Expression language extensions
option(DISABLE_EXPRESSION_LANGUAGE "Disables the scripting extensions." OFF)
include_directories("extensions/expression-language/common")
if (DISABLE_EXPRESSION_LANGUAGE)
    # Build expression language NoOp implementation, if necessary
    include_directories("extensions/expression-language/noop")
    add_subdirectory(extensions/expression-language/noop)
else()
    include_directories("extensions/expression-language/impl")
    createExtension(EXPRESSION-LANGUAGE-EXTENSIONS "EXPRESSION LANGUAGE EXTENSIONS" "This enables NiFi expression language" "extensions/expression-language" "${TEST_DIR}/expression-language-tests")
    if(NOT USE_SYSTEM_CURL)
		message("minifi-expression-language-extensions will depend on curl-external")
		add_dependencies(minifi-expression-language-extensions curl-external)
	endif()
endif()

if(BOOTSTRAP)
	# display an error message for those using the bootstrap
    message(FATAL_ERROR "Bootstrapping is no longer needed within the agent")
endif()

               
add_subdirectory(libminifi)

if(NOT USE_SYSTEM_OPENSSL OR USE_SYSTEM_OPENSSL STREQUAL "OFF")
  add_dependencies(minifi libressl-portable)
endif()

if (WIN32 OR NOT USE_SYSTEM_ZLIB)
	add_dependencies(minifi zlib-external)
endif(WIN32 OR NOT USE_SYSTEM_ZLIB)


createExtension(STANDARD-PROCESSORS "STANDARD PROCESSORS" "Provides standard processors" "extensions/standard-processors" "extensions/standard-processors/tests/")


#### EXTENSIONS
if ((DISABLE_CURL STREQUAL "OFF" OR NOT DISABLE_CURL) AND NOT DISABLE_CIVET)
	createExtension(HTTP-CURL "HTTP CURL" "This enables RESTProtocol, InvokeHTTP, and the HTTPClient for Site to Site" "extensions/http-curl" "extensions/http-curl/tests/")
	if(NOT USE_SYSTEM_CURL)
		message("minifi-http-curl will depend on curl-external")
		add_dependencies(minifi-http-curl curl-external)
	endif()
endif()

option(DISABLE_CIVET "Disables CivetWeb components." OFF)
if (NOT DISABLE_CIVET)
	createExtension(CIVETWEB CIVETWEB "This enables ListenHTTP" "extensions/civetweb" "${TEST_DIR}/civetweb-tests")
endif()

## Add the rocks DB extension
if (NOT ROCKSDB_FOUND OR BUILD_ROCKSDB)
	set(USE_RTTI "TRUE")
	set(BUILD_RD "TRUE")
endif()

option(DISABLE_ROCKSDB "Disables the RocksDB extension." OFF)
if (DISABLE_ROCKSDB STREQUAL "OFF" OR NOT DISABLE_ROCKSDB)
	createExtension(ROCKSDB-REPOS "ROCKSDB REPOS" "This Enables persistent provenance, flowfile, and content repositories using RocksDB" "extensions/rocksdb-repos" "${TEST_DIR}/rocksdb-tests" BUILD_RD "${ROCKSDB_THIRDPARTY_ROOT}")
endif()

## Create LibArchive Extension
if (NOT LibArchive_FOUND OR BUILD_LIBARCHIVE)
	set(BUILD_TP "TRUE")
endif()
option(DISABLE_LIBARCHIVE "Disables the lib archive extensions." OFF)
if (NOT DISABLE_LIBARCHIVE)
	createExtension(ARCHIVE-EXTENSIONS "ARCHIVE EXTENSIONS" "This Enables libarchive functionality including MergeContent, CompressContent, (Un)FocusArchiveEntry and ManipulateArchive." "extensions/libarchive" "${TEST_DIR}/archive-tests" BUILD_TP "thirdparty/libarchive-3.3.2")
endif()

option(ENABLE_GPS "Enables the GPS extension." OFF)
if (ENABLE_ALL OR ENABLE_GPS)
	createExtension(GPS-EXTENSION "GPS EXTENSIONS" "Enables LibGPS Functionality and the GetGPS processor." "extensions/gps" "${TEST_DIR}/gps-tests")
endif()

option(ENABLE_COAP "Enables the CoAP extension." OFF)
if (ENABLE_ALL OR ENABLE_COAP STREQUAL "ON")
	createExtension(COAP-EXTENSION "COAP EXTENSIONS" "Enables LibCOAP Functionality." "extensions/coap" "extensions/coap/tests/")
	if( NOT DISABLE_CURL)
		add_dependencies(minifi-coap minifi-http-curl)
	endif()
endif()

if (WIN32)
option(ENABLE_WEL "Enables the suite of Windows Event Log extensions." OFF)
if (ENABLE_ALL OR ENABLE_WEL)
	createExtension(WEL-EXTENSION "WEL EXTENSIONS" "Enables the suite of Windows Event Log extensions." "extensions/windows-event-log" "${TEST_DIR}/windows-event-log-tests")
endif()
endif(WIN32)

## Create MQTT Extension
option(ENABLE_MQTT "Enables the mqtt extension." OFF)
if(ENABLE_ALL OR ENABLE_MQTT)
        createExtension(MQTT-EXTENSIONS "MQTT EXTENSIONS" "This Enables MQTT functionality including PublishMQTT/ConsumeMQTT" "extensions/mqtt" "${TEST_DIR}/mqtt-tests" "TRUE" "thirdparty/paho.mqtt.c")
endif()

if(ENABLE_ALL OR ENABLE_JNI)
	createExtension(JNI-EXTENSION "JNI EXTENSIONS" "Enables JNI capabilities to support loading Java Classes." "extensions/jni" "${TEST_DIR}/jni-tests")
endif()


option(ENABLE_PCAP "Enables the PCAP extension." OFF)
if(ENABLE_ALL OR ENABLE_PCAP)
	createExtension(PCAP-EXTENSION "PCAP EXTENSIONS" "Enables libPCAP Functionality and the PacketCapture processor." "extensions/pcap" "${TEST_DIR}/pcap-tests")
endif()

## Create LibRdKafka Extension
option(ENABLE_LIBRDKAFKA "Enables the librdkafka extension." OFF)
if (ENABLE_ALL OR ENABLE_LIBRDKAFKA)
	createExtension(RDKAFKA-EXTENSIONS "RDKAFKA EXTENSIONS" "This Enables librdkafka functionality including PublishKafka" "extensions/librdkafka" "${TEST_DIR}/kafka-tests")
endif()

## Scripting extensions
option(DISABLE_SCRIPTING "Disables the scripting extensions." OFF)
if (NOT DISABLE_SCRIPTING)
    createExtension(SCRIPTING-EXTENSIONS "SCRIPTING EXTENSIONS" "This enables scripting" "extensions/script" "${TEST_DIR}/script-tests")
endif()

option(ENABLE_SENSORS "Enables the Sensors package." OFF)
if(ENABLE_ALL OR ENABLE_SENSORS)
	add_subdirectory(thirdparty/RTIMULib/RTIMULib)
	createExtension(SENSOR-EXTENSIONS "SENSOR EXTENSIONS" "Enables the package of sensor extensions." "extensions/sensors" "${TEST_DIR}/sensors-tests")
endif()


## SQLite extensions
option(ENABLE_SQLITE "Disables the scripting extensions." OFF)
if (ENABLE_ALL OR ENABLE_SQLITE)
    createExtension(SQLITE-EXTENSIONS "SQLITE EXTENSIONS" "This enables sqlite" "extensions/sqlite" "${TEST_DIR}/sqlite-tests" "TRUE" "thirdparty/sqlite")
endif()

## USB camera extensions
option(ENABLE_USB_CAMERA "Enables USB camera support." OFF)
if (ENABLE_ALL OR ENABLE_USB_CAMERA)
    createExtension(USB-CAMERA-EXTENSIONS "USB CAMERA EXTENSIONS" "This enables USB camera support" "extensions/usb-camera" "${TEST_DIR}/usb-camera-tests" "TRUE" "thirdparty/libuvc-0.0.6")
endif()

## TensorFlow extensions
## Disabled by default because TF can be complex/environment-specific to build
option(ENABLE_TENSORFLOW "Disables the TensorFlow extensions." OFF)
if (ENABLE_TENSORFLOW)
    createExtension(TENSORFLOW-EXTENSIONS "TENSORFLOW EXTENSIONS" "This enables TensorFlow support" "extensions/tensorflow" "${TEST_DIR}/tensorflow-tests")
endif()

## AWS Extentions
if (ENABLE_AWS)
	createExtension(AWS-EXTENSIONS "AWS EXTENSIONS" "This enables AWS support" "extensions/aws" )
endif()

## OpenCV Extesions
option(ENABLE_OPENCV "Disables the OpenCV extensions." OFF)
if (ENABLE_OPENCV)
	createExtension(OPENCV-EXTENSIONS "OPENCV EXTENSIONS" "This enabled OpenCV support" "extensions/opencv" "${TEST_DIR}/opencv-tests")
endif()

## Bustache/template extensions
option(ENABLE_BUSTACHE "Enables Bustache (ApplyTemplate) support." OFF)
if (ENABLE_BUSTACHE)
    createExtension(BUSTACHE-EXTENSIONS "BUSTACHE EXTENSIONS" "This enables bustache functionality including ApplyTemplate." "extensions/bustache" "${TEST_DIR}/bustache-tests" "TRUE" "thirdparty/bustache")
endif()

## SFTP extensions
option(ENABLE_SFTP "Enables SFTP support." OFF)
if ((ENABLE_ALL OR ENABLE_SFTP) AND NOT DISABLE_CURL)
    if(WIN32 OR NOT USE_SYSTEM_LIBSSH2)
        include(LibSSH2)
        use_bundled_libssh2(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libssh2/dummy")
    else()
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libssh2/sys")
        find_package (LibSSH2 REQUIRED)
    endif()

    createExtension(SFTP "SFTP EXTENSIONS" "This enables SFTP support" "extensions/sftp" "extensions/sftp/tests")

    if(NOT USE_SYSTEM_LIBSSH2)
        message("minifi-sftp will depend on libssh2-external")
        add_dependencies(minifi-sftp libssh2-external)
    endif()
    if(NOT USE_SYSTEM_CURL)
        message("minifi-sftp will depend on curl-external")
        add_dependencies(minifi-sftp curl-external)
    endif()
    if(NOT USE_SYSTEM_ZLIB)
        message("minifi-sftp will depend on zlib-external")
        add_dependencies(minifi-sftp zlib-external)
    endif()
endif()

## NOW WE CAN ADD LIBRARIES AND EXTENSIONS TO MAIN

add_subdirectory(main)
add_subdirectory(nanofi)

add_dependencies(nanofi minifiexe)

if (NOT DISABLE_CURL AND NOT DISABLE_CONTROLLER)
	add_subdirectory(thirdparty/cxxopts)
	add_subdirectory(controller)
	add_dependencies(minificontroller minifiexe)
endif()


if (NOT DISABLE_CURL)
  if (ENABLE_PYTHON)
  	if (NOT WIN32)
  		add_subdirectory(python/library)
  	endif()	
  endif(ENABLE_PYTHON)
endif()

get_property(selected_extensions GLOBAL PROPERTY EXTENSION-OPTIONS)

# Generate the build identifier if one is not provided
if (NOT BUILD_IDENTIFIER)
     string(RANDOM LENGTH 24 BUILD_IDENTIFIER)
     set(BUILD_IDENTIFIER "${BUILD_IDENTIFIER}" CACHE STRING "Build identifier" FORCE)
endif()

message("BUILD_IDENTIFIER is ${BUILD_IDENTIFIER}")

if (WIN32)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE BUILD_REV
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
	execute_process(COMMAND 
			"${CMAKE_CURRENT_SOURCE_DIR}/generateVersion.bat" 
			"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" 
			${CMAKE_CURRENT_SOURCE_DIR} 
			${CMAKE_CURRENT_SOURCE_DIR}/libminifi/include/agent/ 
			"${CMAKE_CXX_COMPILER}"
			"${CMAKE_CXX_COMPILER_VERSION}" 
			"${CMAKE_CXX_FLAGS}" 
			\"${selected_extensions}\"
			"${BUILD_IDENTIFIER}"
			"${BUILD_REV}")
else()
	execute_process(COMMAND 
		"${CMAKE_CURRENT_SOURCE_DIR}/generateVersion.sh" 
		"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" 
		${CMAKE_CURRENT_SOURCE_DIR} 
		${CMAKE_CURRENT_SOURCE_DIR}/libminifi/include/agent/ 
		"${CMAKE_CXX_COMPILER}"
		"${CMAKE_CXX_COMPILER_VERSION}" 
		"${CMAKE_CXX_FLAGS}" 
		"${selected_extensions}" 
		"${BUILD_IDENTIFIER}")

endif()
# Generate source assembly
set(ASSEMBLY_BASE_NAME "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
if(WIN32)
set(CPACK_ALL_INSTALL_TYPES Full Developer)
set(CPACK_COMPONENT_LIBRARIES_INSTALL_TYPES Developer Full)
set(CPACK_COMPONENT_HEADERS_INSTALL_TYPES Developer Full)
set(CPACK_COMPONENT_APPLICATIONS_INSTALL_TYPES Full)
set(CPACK_WIX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWin64.wsi")
else()
set(CPACK_SOURCE_GENERATOR "TGZ")
endif(WIN32)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}-source")
set(CPACK_SOURCE_IGNORE_FILES "/docs/generated/;${CMAKE_SOURCE_DIR}/build/;~$;${CPACK_SOURCE_IGNORE_FILES};${CMAKE_SOURCE_DIR}/.git/;${CMAKE_SOURCE_DIR}/.idea/;${CMAKE_SOURCE_DIR}/cmake-build-debug/;${CMAKE_SOURCE_DIR}/extensions/expression-language/Scanner.h;${CMAKE_SOURCE_DIR}/extensions/expression-language/Scanner.cpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/Parser.cpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/Parser.hpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/scanner.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/position.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/location.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/stack.hh")

# Generate binary assembly
install(FILES conf/minifi.properties conf/minifi-log.properties conf/minifi-uid.properties conf/config.yml
        DESTINATION conf
        COMPONENT bin)
        
install(DIRECTORY extensions/pythonprocessors/
        DESTINATION minifi-python
        COMPONENT bin)

install(PROGRAMS bin/minifi.sh
        DESTINATION bin
        COMPONENT bin)

install(FILES LICENSE README.md NOTICE
        DESTINATION .
        COMPONENT bin)

if(WIN32)
#preference is to use the exe type so that we have a solution that works well for cross compilation
#but that leaves the onus up to the developer, so until we can automate some of that build let's enforce
#the exe here temporarily
#TODO: Remove this and automate this step.
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/main/minifi.exe
        DESTINATION bin
        COMPONENT bin)
else()
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/main/minifi
        DESTINATION bin
        COMPONENT bin)
        
endif()


if (WIN32)
set(CPACK_GENERATOR "WIX")
else()
set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY 1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apache NiFi MiNiFi C++ version ${VERSION}")
set(CPACK_PACKAGE_VENDOR "Apache NiFi")
# set version information
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/msi/LICENSE.txt")
set(CPACK_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}")
if(NOT WIN32)
set(CPACK_GENERATOR "TGZ")
set(CPACK_BINARY_TGZ, "ON")
endif()
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_RDKAFKA_COMPONENT_INSTALL ON)
set(CPACK_MQTT_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL bin conf)


include(CPack)
### include modules

if (NOT SKIP_TESTS)
	include(BuildTests)

    # BENCHMARKING depends on test support code
    if (ENABLE_BENCHMARKING)
        set(BENCHMARK_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/google-benchmark-install/lib64")
        set(BENCHMARK_BYPRODUCT "${BENCHMARK_LIB_DIR}/libbenchmark.a")
        set(BENCHMARK_MAIN_BYPRODUCT "${BENCHMARK_LIB_DIR}/libbenchmark.a")
        #  GIT_REPOSITORY "https://github.com/google/benchmark.git"
        #GIT_TAG "090faecb454fbd6e6e17a75ef8146acb037118d4"  # Version 1.5.0
        ExternalProject_Add(
                google-benchmark-external
                SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/benchmark-1.5.0"
                CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/google-benchmark-install"
                "-DBENCHMARK_ENABLE_TESTING=OFF"
                BUILD_BYPRODUCTS ${BENCHMARK_BYPRODUCT} ${BENCHMARK_MAIN_BYPRODUCT}
        )
        add_library(benchmark STATIC IMPORTED)
        set_target_properties(benchmark PROPERTIES IMPORTED_LOCATION ${BENCHMARK_BYPRODUCT})
        add_dependencies(benchmark google-benchmark-external)
        add_library(benchmark_main STATIC IMPORTED)
        set_target_properties(benchmark_main PROPERTIES IMPORTED_LOCATION ${BENCHMARK_MAIN_BYPRODUCT})
        add_dependencies(benchmark_main google-benchmark-external)
        file(GLOB LIBMINIFI_BENCHMARKS  "libminifi/benchmark/*.cpp")
        set(ALL_BENCHMARKS "${LIBMINIFI_BENCHMARKS}")

        set(BENCHMARK_COUNT 0)
        foreach(benchmarkfile ${ALL_BENCHMARKS})
            get_filename_component(benchmarkfilename "${benchmarkfile}" NAME_WE)
            add_executable("${benchmarkfilename}" "${benchmarkfile}")
            target_link_libraries("${benchmarkfilename}" benchmark benchmark_main "${CMAKE_THREAD_LIBS_INIT}")
            target_link_libraries ("${benchmarkfilename}" -Wl,--whole-archive core-minifi minifi -Wl,--no-whole-archive)
            appendIncludes("${benchmarkfilename}")
	    if (DISABLE_ROCKSDB STREQUAL "OFF" OR NOT DISABLE_ROCKSDB)
	      target_include_directories("${benchmarkfilename}" BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rocksdb/include")
	      target_include_directories("${benchmarkfilename}" BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extensions/rocksdb-repos")
	      target_link_libraries ("${benchmarkfilename}" -Wl,--whole-archive minifi-rocksdb-repos -Wl,--no-whole-archive)
	      add_definitions(-DENABLE_ROCKSDB_BENCHMARKS=1)
	    endif()
            target_include_directories("${benchmarkfilename}" BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/benchmark-1.5.0/include")
            math(EXPR BENCHMARK_COUNT "${BENCHMARK_COUNT}+1")
        endforeach()
        message("-- Finished building ${BENCHMARK_COUNT} benchmark file(s)...")
    endif()
endif()

include(BuildDocs)

include(DockerConfig)

if(NOT WIN32)
# Create a custom build target that will run the linter.
get_property(extensions GLOBAL PROPERTY EXTENSION-LINTERS)
add_custom_target(linter
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/ --
            ${CMAKE_SOURCE_DIR}/libminifi/src/
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/ --
            ${CMAKE_SOURCE_DIR}/libminifi/test/
    DEPENDS ${extensions})
endif(NOT WIN32)

# Custom target to download and run Apache Release Audit Tool (RAT)
add_custom_target(
        apache-rat
        ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/RunApacheRAT.cmake
        COMMAND java -jar ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat/apache-rat-0.13/apache-rat-0.13.jar -E ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat/.rat-excludes -d ${CMAKE_SOURCE_DIR} | grep -B 1 -A 15 Summary )

feature_summary(WHAT ALL FILENAME ${CMAKE_BINARY_DIR}/all.log)
