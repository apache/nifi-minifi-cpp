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

cmake_minimum_required(VERSION 3.7)
cmake_policy(SET CMP0065 OLD) # default export policy, required for self-dlopen

project(nifi-minifi-cpp)
set(PROJECT_NAME "nifi-minifi-cpp")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 7)
set(PROJECT_VERSION_PATCH 0)

include(CMakeDependentOption)
include(CheckIncludeFile)
include(FeatureSummary)
include(ExternalProject)

# Provide custom modules for the project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(WholeArchive)

option(SKIP_TESTS "Skips building all tests." OFF)
option(PORTABLE "Instructs the compiler to remove architecture specific optimizations" ON)
option(USE_SHARED_LIBS "Builds using shared libraries" ON)
option(ENABLE_PYTHON "Instructs the build system to enable building shared objects for the python lib" OFF)
cmake_dependent_option(STATIC_BUILD "Attempts to statically link as many dependencies as possible." ON "NOT ENABLE_PYTHON; NOT USE_SHARED_LIBS" OFF)  
option(LIBC_STATIC "Instructs the build system to statically link libstdc++ and glibc into minifiexe. Experiemental" OFF)
option(OPENSSL_OFF "Disables OpenSSL" OFF)
option(ENABLE_OPS "Enable Operations/zlib Tools" ON)
option(ENABLE_JNI "Instructs the build system to enable the JNI extension" OFF)
option(ENABLE_OPENCV "Instructs the build system to enable the OpenCV extension" OFF)
option(ENABLE_OPC "Instructs the build system to enable the OPC extension" OFF)
option(BUILD_SHARED_LIBS "Build yaml cpp shared lib" OFF)

option(BUILD_ROCKSDB "Instructs the build system to use RocksDB from the third party directory" ON)
option(FORCE_WINDOWS "Instructs the build system to force Windows builds when WIN32 is specified" OFF)
option(DISABLE_CURL "Disables libCurl Properties." OFF)

option(USE_GOLD_LINKER "Use Gold Linker" OFF)
option(INSTALLER_MERGE_MODULES "Creates installer with merge modules" OFF)
# This is needed for ninja:
# By default, neither Clang or GCC will add ANSI-formatted colors to your output if they detect
# the output medium is not a terminal. This means no coloring when using a generator
# different than "GNU Makefiles".
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)
if (${FORCE_COLORED_OUTPUT})
	message("CMAKE_CXX_COMPILER_ID is ${CMAKE_CXX_COMPILER_ID}")
	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	   add_compile_options (-fdiagnostics-color=always)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	   add_compile_options (-fcolor-diagnostics)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
	   add_compile_options (-fcolor-diagnostics)
	endif ()
endif ()

cmake_dependent_option(ASAN_BUILD "Uses AddressSanitizer to instrument the code" OFF "NOT WIN32" OFF)

# Option: STRICT_GSL_CHECKS
# AUDIT: Enable all checks, including gsl_ExpectsAudit() and gsl_EnsuresAudit()
# ON: Enable all checks, excluding gsl_ExpectsAudit() and gsl_EnsuresAudit() (GSL default)
# DEBUG_ONLY: Like ON in the Debug configuration, OFF in others (MiNiFi C++ default)
# OFF: Throw on contract checking and assertion failures instead of calling std::terminate()
set(STRICT_GSL_CHECKS "DEBUG_ONLY" CACHE STRING "Contract checking and assertion failures call terminate")
list(APPEND STRICT_GSL_CHECKS_Values AUDIT ON DEBUG_ONLY OFF)
set_property(CACHE STRICT_GSL_CHECKS PROPERTY STRINGS ${STRICT_GSL_CHECKS_Values})

# Use ccache if present
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    message("-- Found ccache: ${CCACHE_FOUND}")
endif(CCACHE_FOUND)

# Use gold linker if instructed
if (UNIX AND USE_GOLD_LINKER AND NOT APPLE )
  execute_process(COMMAND ${CMAKE_C_COMPILER} -fuse-ld=gold -Wl,--version ERROR_QUIET OUTPUT_VARIABLE ld_version)
  if ("${ld_version}" MATCHES "GNU gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold -Wl,--disable-new-dtags")
  endif()
endif()

# Check for exec info before we enable the backtrace features.
CHECK_INCLUDE_FILE("execinfo.h" HAS_EXECINFO)
if (ENABLE_OPS AND HAS_EXECINFO AND NOT WIN32)
  add_definitions("-DHAS_EXECINFO=1")
endif()

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
include(CheckCXXCompilerFlag)
if (WIN32)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_compile_options("/W1")
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

if (WIN32)
	add_definitions(-DSERVICE_NAME="Apache NiFi MINiFi")
endif()

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if (NOT PORTABLE)
	if(MSVC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
	endif()
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# thread library - we must find this before we set ASAN flags, otherwise FindThreads.cmake sometimes thinks we don't
# need to link with libpthread because libasan already provides some overridden pthread_* symbols.
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
message("CMAKE_THREAD_LIBS_INIT is ${CMAKE_THREAD_LIBS_INIT}")
message("THREADS_HAVE_PTHREAD_ARG is ${THREADS_HAVE_PTHREAD_ARG}")

# We want everything PIC. Even though it isn't needed for the executables itself (that use libminifi.a), it is needed
# to create libminifi.so, and it will enable us to use the same set of compiled libminifi sources for both.
# It has the added benefit of having PIE executables that supports using Address space layout randomization,
# an important security feature.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use ASAN if instructed
if (ASAN_BUILD)
	set(ASAN_FLAGS "-g -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ASAN_FLAGS}")
endif()

#### Third party dependencies ####

# Define function for passing dependencies
function(append_third_party_passthrough_args OUTPUT EXTERNALPROJECT_CMAKE_ARGS)
	string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH}")
	list(APPEND EXTERNALPROJECT_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
	list(APPEND EXTERNALPROJECT_CMAKE_ARGS ${PASSTHROUGH_VARIABLES})
	set(${OUTPUT} ${EXTERNALPROJECT_CMAKE_ARGS} PARENT_SCOPE)
endfunction()

# Find patch executable
find_package(Patch REQUIRED)

# Setup passthrough args
set(PASSTHROUGH_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI}
		-DANDROID_PLATFORM=${ANDROID_PLATFORM}
		-DANDROID_STL=${ANDROID_STL}
		-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DANDROID_NDK=${ANDROID_NDK}
		-DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
		-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
		-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
		-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
		-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}
		-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM}
		-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}
		-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		-G${CMAKE_GENERATOR}
		)

# jemalloc
if(NOT WIN32)
	if (ENABLE_JNI)
		if (NOT DISABLE_JEMALLOC)
			include(BundledJemalloc)
			use_bundled_jemalloc(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
			message("jemalloc found at ${JEMALLOC_LIBRARIES}")
		endif()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_JNI")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_JNI")
	endif()
endif()

# Simple-Windows-Posix-Semaphore
if (WIN32)
  	add_subdirectory("thirdparty/Simple-Windows-Posix-Semaphore")
endif()

# ossp-uuid
if(NOT WIN32)
	include(BundledOSSPUUID)
	use_bundled_osspuuid(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
endif()

# OpenSSL/LibreSSL
if (NOT OPENSSL_OFF)
	include(BundledLibreSSL)
	use_libre_ssl("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/ssl")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENSSL_SUPPORT")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOPENSSL_SUPPORT")
endif()

# zlib
include(BundledZLIB)
use_bundled_zlib(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/zlib/dummy")

# uthash
add_library(ut INTERFACE)
target_include_directories(ut INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ut")

# cURL
if(NOT DISABLE_CURL)
	include(BundledLibcURL)
	use_bundled_curl(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/curl/dummy")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDISABLE_CURL")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDISABLE_CURL")
endif()

# spdlog
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/spdlog-20170710/include")
if (NOT WIN32)
	set_property(TARGET spdlog APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_ENABLE_SYSLOG")
endif()

# yaml-cpp
include(BundledYamlCpp)
use_bundled_yamlcpp(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# concurrentqueue
add_library(concurrentqueue INTERFACE)
target_include_directories(concurrentqueue INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/concurrentqueue")

# RapidJSON
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rapidjson-1.1.0/include")

# Cron
add_library(cron INTERFACE)
target_include_directories(cron INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cron")

# cxxopts
add_library(cxxopts INTERFACE)
target_include_directories(cxxopts INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cxxopts/include")

# gsl-lite
add_library(gsl-lite INTERFACE)
target_include_directories(gsl-lite INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/gsl-lite-0.37.0/include")

# Add necessary definitions based on the value of STRICT_GSL_CHECKS, see gsl-lite README for more details
list(APPEND GslDefinitions gsl_CONFIG_DEFAULTS_VERSION=1)
list(APPEND GslDefinitionsNonStrict gsl_CONFIG_CONTRACT_VIOLATION_THROWS gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION=1)
if (STRICT_GSL_CHECKS STREQUAL "AUDIT")
	list(APPEND GslDefinitions gsl_CONFIG_CONTRACT_CHECKING_AUDIT)
endif()
if (NOT STRICT_GSL_CHECKS)  # OFF (or any other falsey string) matches, AUDIT/ON/DEBUG_ONLY don't match
	list(APPEND GslDefinitions ${GslDefinitionsNonStrict})
endif()
if (STRICT_GSL_CHECKS STREQUAL "DEBUG_ONLY")
	list(APPEND GslDefinitions $<$<NOT:$<CONFIG:Debug>>:${GslDefinitionsNonStrict}>)
endif()
target_compile_definitions(gsl-lite INTERFACE ${GslDefinitions})

# optional-lite
add_library(optional-lite INTERFACE)
target_include_directories(optional-lite INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/optional-lite-3.2.0/include")

#### Extensions ####
SET(TEST_DIR ${CMAKE_SOURCE_DIR}/libminifi/test)
include(Extensions)

if(BOOTSTRAP)
	# display an error message for those using the bootstrap
    message(FATAL_ERROR "Bootstrapping is no longer needed within the agent")
endif()

add_subdirectory(libminifi)

createExtension(STANDARD-PROCESSORS "STANDARD PROCESSORS" "Provides standard processors" "extensions/standard-processors" "extensions/standard-processors/tests/")

if ((DISABLE_CURL STREQUAL "OFF" OR NOT DISABLE_CURL) AND NOT DISABLE_CIVET)
	createExtension(HTTP-CURL "HTTP CURL" "This enables RESTProtocol, InvokeHTTP, and the HTTPClient for Site to Site" "extensions/http-curl" "extensions/http-curl/tests/")
	message("minifi-http-curl will depend on curl-external")
endif()

option(DISABLE_EXPRESSION_LANGUAGE "Disables the scripting extensions." OFF)
if (NOT DISABLE_EXPRESSION_LANGUAGE)
    createExtension(EXPRESSION-LANGUAGE-EXTENSIONS "EXPRESSION LANGUAGE EXTENSIONS" "This enables NiFi expression language" "extensions/expression-language" "extensions/expression-language/tests")

	message("minifi-expression-language-extensions will depend on curl-external")
endif()

option(DISABLE_CIVET "Disables CivetWeb components." OFF)
if (NOT DISABLE_CIVET)
	include(BundledCivetWeb)
	use_bundled_civetweb(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/civetweb/dummy")

	createExtension(CIVETWEB CIVETWEB "This enables ListenHTTP" "extensions/civetweb" "extensions/civetweb/tests")
endif()

## Add the rocks DB extension
option(DISABLE_ROCKSDB "Disables the RocksDB extension." OFF)
if (NOT DISABLE_ROCKSDB)
	if (BUILD_ROCKSDB)
		include(BundledRocksDB)
		use_bundled_rocksdb(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	else()
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/rocksdb/sys")
		find_package(RocksDB REQUIRED)
	endif()
	createExtension(ROCKSDB-REPOS "ROCKSDB REPOS" "This Enables persistent provenance, flowfile, and content repositories using RocksDB" "extensions/rocksdb-repos" "${TEST_DIR}/rocksdb-tests")
endif()

## Create LibArchive Extension
option(DISABLE_LIBARCHIVE "Disables the lib archive extensions." OFF)
option(DISABLE_LZMA "Disables the liblzma build" OFF)
option(DISABLE_BZIP2 "Disables the bzip2 build" OFF)
if (NOT DISABLE_LIBARCHIVE)
	if (NOT DISABLE_LZMA)
		include(BundledLibLZMA)
		use_bundled_liblzma(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/liblzma/dummy")
	endif()

	if (NOT DISABLE_BZIP2)
		include(BundledBZip2)
		use_bundled_bzip2(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
		list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/bzip2/dummy")
	endif()

	include(BundledLibArchive)
	use_bundled_libarchive(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

	createExtension(ARCHIVE-EXTENSIONS "ARCHIVE EXTENSIONS" "This Enables libarchive functionality including MergeContent, CompressContent, (Un)FocusArchiveEntry and ManipulateArchive." "extensions/libarchive" "${TEST_DIR}/archive-tests")
endif()

option(ENABLE_GPS "Enables the GPS extension." OFF)
if (ENABLE_ALL OR ENABLE_GPS)
	createExtension(GPS-EXTENSION "GPS EXTENSIONS" "Enables LibGPS Functionality and the GetGPS processor." "extensions/gps" "${TEST_DIR}/gps-tests")
endif()

option(ENABLE_COAP "Enables the CoAP extension." OFF)
if (ENABLE_ALL OR ENABLE_COAP STREQUAL "ON")
	include(BundledLibCoAP)
	use_bundled_libcoap(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	createExtension(COAP-EXTENSION "COAP EXTENSIONS" "Enables LibCOAP Functionality." "extensions/coap" "extensions/coap/tests/")
	if( NOT DISABLE_CURL)
		add_dependencies(minifi-coap minifi-http-curl)
	endif()
endif()

if (WIN32)
	option(ENABLE_WEL "Enables the suite of Windows Event Log extensions." OFF)
	if (ENABLE_ALL OR ENABLE_WEL)
		include(BundledPugiXml)
		use_bundled_pugixml(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
		createExtension(WEL-EXTENSION "WEL EXTENSIONS" "Enables the suite of Windows Event Log extensions." "extensions/windows-event-log" "extensions/windows-event-log/tests")
	endif()
endif(WIN32)

option(ENABLE_SQL "Enables the SQL Suite of Tools." OFF)
if (ENABLE_ALL OR ENABLE_SQL)
	createExtension(SQL-EXTENSIONS "SQL EXTENSIONS" "Enables the SQL Suite of Tools" "extensions/sql")
endif()

## Create MQTT Extension
option(ENABLE_MQTT "Enables the mqtt extension." OFF)
if(ENABLE_ALL OR ENABLE_MQTT)
		include(BundledPahoMqttC)
		use_bundled_pahomqttc(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
        createExtension(MQTT-EXTENSIONS "MQTT EXTENSIONS" "This Enables MQTT functionality including PublishMQTT/ConsumeMQTT" "extensions/mqtt" "${TEST_DIR}/mqtt-tests")
endif()

# Create JNI Extension
if(ENABLE_ALL OR ENABLE_JNI)
	createExtension(JNI-EXTENSION "JNI EXTENSIONS" "Enables JNI capabilities to support loading Java Classes." "extensions/jni" "${TEST_DIR}/jni-tests")
endif()

# Create PCAP Extension
option(ENABLE_PCAP "Enables the PCAP extension." OFF)
if(ENABLE_ALL OR ENABLE_PCAP)
	createExtension(PCAP-EXTENSION "PCAP EXTENSIONS" "Enables libPCAP Functionality and the PacketCapture processor." "extensions/pcap" "${TEST_DIR}/pcap-tests")
endif()

## Create LibRdKafka Extension
option(ENABLE_LIBRDKAFKA "Enables the librdkafka extension." OFF)
if (ENABLE_ALL OR ENABLE_LIBRDKAFKA)
	include(BundledLibRdKafka)
	use_bundled_librdkafka(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	createExtension(RDKAFKA-EXTENSIONS "RDKAFKA EXTENSIONS" "This Enables librdkafka functionality including PublishKafka" "extensions/librdkafka" "extensions/librdkafka/tests")
endif()

## Scripting extensions
option(DISABLE_SCRIPTING "Disables the scripting extensions." OFF)
if (NOT DISABLE_SCRIPTING)
	# sol
	add_library(sol INTERFACE)
	target_include_directories(sol INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sol2-2.20.0")
    createExtension(SCRIPTING-EXTENSIONS "SCRIPTING EXTENSIONS" "This enables scripting" "extensions/script" "${TEST_DIR}/script-tests")
endif()

# Sensors extensions
option(ENABLE_SENSORS "Enables the Sensors package." OFF)
if(ENABLE_ALL OR ENABLE_SENSORS)
	add_subdirectory(thirdparty/RTIMULib/RTIMULib)
	createExtension(SENSOR-EXTENSIONS "SENSOR EXTENSIONS" "Enables the package of sensor extensions." "extensions/sensors" "${TEST_DIR}/sensors-tests")
endif()


## SQLite extensions
option(ENABLE_SQLITE "Disables the scripting extensions." OFF)
if (ENABLE_SQLITE)
	include(BundledSQLite)
	use_bundled_sqlite(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    createExtension(SQLITE-EXTENSIONS "SQLITE EXTENSIONS" "This enables sqlite" "extensions/sqlite" "${TEST_DIR}/sqlite-tests")
endif()

if (ENABLE_SQL AND ENABLE_SQLITE)
	message(FATAL_ERROR "ENABLE_SQL and ENABLE_SQLITE are incompatible. Set only one at a time.")
endif()

## USB camera extensions
option(ENABLE_USB_CAMERA "Enables USB camera support." OFF)
if (ENABLE_ALL OR ENABLE_USB_CAMERA)
	include(BundledLibUvc)
	use_bundled_libuvc(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    createExtension(USB-CAMERA-EXTENSIONS "USB CAMERA EXTENSIONS" "This enables USB camera support" "extensions/usb-camera" "${TEST_DIR}/usb-camera-tests")
endif()

## TensorFlow extensions
## Disabled by default because TF can be complex/environment-specific to build
option(ENABLE_TENSORFLOW "Disables the TensorFlow extensions." OFF)
if (ENABLE_TENSORFLOW)
    createExtension(TENSORFLOW-EXTENSIONS "TENSORFLOW EXTENSIONS" "This enables TensorFlow support" "extensions/tensorflow" "${TEST_DIR}/tensorflow-tests")
endif()

## AWS Extentions
option(ENABLE_AWS "Enables AWS support." OFF)
if (ENABLE_AWS)
	include(BundledAwsSdkCpp)
	use_bundled_libaws(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	createExtension(AWS-EXTENSIONS "AWS EXTENSIONS" "This enables AWS support" "extensions/aws")
endif()

## OpenCV Extesions
option(ENABLE_OPENCV "Disables the OpenCV extensions." OFF)
if (ENABLE_OPENCV)
	createExtension(OPENCV-EXTENSIONS "OPENCV EXTENSIONS" "This enabled OpenCV support" "extensions/opencv" "extensions/opencv/tests")
endif()


## Bustache/template extensions
option(ENABLE_BUSTACHE "Enables Bustache (ApplyTemplate) support." OFF)
if (ENABLE_BUSTACHE)
	include(BundledBustache)
	use_bundled_bustache(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    createExtension(BUSTACHE-EXTENSIONS "BUSTACHE EXTENSIONS" "This enables bustache functionality including ApplyTemplate." "extensions/bustache" "${TEST_DIR}/bustache-tests")
endif()

## OPC Extentions
if (ENABLE_OPC)
	include(BundledMbedTLS)
	use_bundled_mbedtls(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/mbedtls/dummy")

	include(BundledOpen62541)
	use_bundled_open62541(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

	createExtension(OPC-EXTENSIONS "OPC EXTENSIONS" "This enables OPC-UA support" "extensions/opc")
endif()

## SFTP extensions
option(ENABLE_SFTP "Enables SFTP support." OFF)
if ((ENABLE_ALL OR ENABLE_SFTP) AND NOT DISABLE_CURL)
	include(BundledLibSSH2)
	use_bundled_libssh2(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/libssh2/dummy")

    createExtension(SFTP "SFTP EXTENSIONS" "This enables SFTP support" "extensions/sftp" "extensions/sftp/tests")
endif()

## Openwsman Extesions
option(ENABLE_OPENWSMAN "Enables the Openwsman extensions." OFF)
if (ENABLE_OPENWSMAN AND NOT DISABLE_CIVET AND NOT DISABLE_CURL)
	include(BundledLibXml2)
	use_bundled_libxml2(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/libxml2/dummy")

	include(BundledOpenWSMAN)
	use_bundled_openwsman(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    createExtension(OPENWSMAN-EXTENSIONS "OPENWSMAN EXTENSIONS" "This enables Openwsman support" "extensions/openwsman")
endif()

## NOW WE CAN ADD LIBRARIES AND EXTENSIONS TO MAIN
add_subdirectory(main)
add_subdirectory(nanofi)

add_dependencies(nanofi minifiexe)

if (NOT DISABLE_CURL AND NOT DISABLE_CONTROLLER)
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
			OUTPUT_STRIP_TRAILING_WHITESPACE)
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
			${CMAKE_CURRENT_SOURCE_DIR}/libminifi/src/agent/
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
	set(CPACK_WIX_EXTENSIONS WixUtilExtension)
	set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/msi/minifi-logo-png-banner.png")
	set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/msi/bgr.png")

	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/conf/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/conf/")
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

	# Determine the path of the VC Redistributable Merge Modules
	if (DEFINED ENV{VCToolsRedistDir})
		# Just get the redist dir that has been set by the build environment
		set(VCRUNTIME_REDIST_DIR $ENV{VCToolsRedistDir})
	else()
		# Try to fall back to a redist dir relative to the MSVC compiler
		string(REGEX REPLACE "/VC/Tools/.*" "/VC/Redist/MSVC/" VCRUNTIME_REDIST_BASE_DIR ${CMAKE_C_COMPILER})
		file(GLOB VCRUNTIME_REDIST_VERSIONS "${VCRUNTIME_REDIST_BASE_DIR}/*")
		if (NOT VCRUNTIME_REDIST_VERSIONS)
			message(FATAL_ERROR "Could not find the VC Redistributable Merge Modules. Please set VCRUNTIME_X86_MERGEMODULE_PATH and VCRUNTIME_X64_MERGEMODULE_PATH manually!")
		endif()
		# Sort the directories in descending order and take the first one - it should be the latest version
		list(SORT VCRUNTIME_REDIST_VERSIONS)
		list(REVERSE VCRUNTIME_REDIST_VERSIONS)
		list(GET VCRUNTIME_REDIST_VERSIONS 0 VCRUNTIME_REDIST_DIR)
	endif()

    if (INSTALLER_MERGE_MODULES)
        message("Creating installer with merge modules")
        file(GLOB VCRUNTIME_X86_MERGEMODULES "${VCRUNTIME_REDIST_DIR}/MergeModules/Microsoft_VC*_CRT_x86.msm")
        file(GLOB VCRUNTIME_X64_MERGEMODULES "${VCRUNTIME_REDIST_DIR}/MergeModules/Microsoft_VC*_CRT_x64.msm")
        if (NOT VCRUNTIME_X86_MERGEMODULES OR NOT VCRUNTIME_X64_MERGEMODULES)
            message(FATAL_ERROR "Could not find the VC Redistributable Merge Modules. Please set VCRUNTIME_X86_MERGEMODULE_PATH and VCRUNTIME_X64_MERGEMODULE_PATH manually!")
        else()
            list(GET VCRUNTIME_X86_MERGEMODULES 0 VCRUNTIME_X86_MERGEMODULE_PATH)
            list(GET VCRUNTIME_X64_MERGEMODULES 0 VCRUNTIME_X64_MERGEMODULE_PATH)
        endif()

        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            message("Using ${VCRUNTIME_X64_MERGEMODULE_PATH} VC Redistributable Merge Module")
            configure_file("msi/x64.wsi" "msi/x64.wsi" @ONLY)
            set(CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/msi/x64.wsi")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            message("Using ${VCRUNTIME_X86_MERGEMODULE_PATH} VC Redistributable Merge Module")
            configure_file("msi/x86.wsi" "msi/x86.wsi" @ONLY)
            set(CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/msi/x86.wsi")
        else()
            message(FATAL_ERROR "Could not determine architecture, CMAKE_SIZEOF_VOID_P is unexpected: ${CMAKE_SIZEOF_VOID_P}")
        endif()
        set(CPACK_WIX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWinMergeModules.wsi")
    else()
        message("Creating installer with redistributables")
        file(GLOB VCRUNTIME_X86_REDIST_CRT_DIR "${VCRUNTIME_REDIST_DIR}/x86/Microsoft.VC141.CRT")
        file(GLOB VCRUNTIME_X64_REDIST_CRT_DIR "${VCRUNTIME_REDIST_DIR}/x64/Microsoft.VC141.CRT")

        if (NOT VCRUNTIME_X86_REDIST_CRT_DIR OR NOT VCRUNTIME_X64_REDIST_CRT_DIR)
            message(FATAL_ERROR "Could not find the VC Redistributable. Please set VCRUNTIME_X86_REDIST_CRT_DIR and VCRUNTIME_X64_REDIST_CRT_DIR manually!")
        endif()

        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            message("Using ${VCRUNTIME_X64_REDIST_CRT_DIR} VC Redistributables")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/concrt140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/msvcp140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/msvcp140_1.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/msvcp140_2.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/vccorlib140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X64_REDIST_CRT_DIR}/vcruntime140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            message("Using ${VCRUNTIME_X86_REDIST_CRT_DIR} VC Redistributables")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/concrt140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/msvcp140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/msvcp140_1.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/msvcp140_2.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/vccorlib140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
            file(COPY "${VCRUNTIME_X86_REDIST_CRT_DIR}/vcruntime140.dll" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
        else()
            message(FATAL_ERROR "Could not determine architecture, CMAKE_SIZEOF_VOID_P is unexpected: ${CMAKE_SIZEOF_VOID_P}")
        endif()
        set(CPACK_WIX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWin.wsi")
    endif()
else()
	set(CPACK_SOURCE_GENERATOR "TGZ")
endif(WIN32)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}-source")
set(CPACK_SOURCE_IGNORE_FILES "/docs/generated/;${CMAKE_SOURCE_DIR}/build/;~$;${CPACK_SOURCE_IGNORE_FILES};${CMAKE_SOURCE_DIR}/.git/;${CMAKE_SOURCE_DIR}/.idea/;${CMAKE_SOURCE_DIR}/cmake-build-debug/;${CMAKE_SOURCE_DIR}/extensions/expression-language/Scanner.h;${CMAKE_SOURCE_DIR}/extensions/expression-language/Scanner.cpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/Parser.cpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/Parser.hpp;${CMAKE_SOURCE_DIR}/extensions/expression-language/scanner.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/position.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/location.hh;${CMAKE_SOURCE_DIR}/extensions/expression-language/stack.hh")

# Generate binary assembly. Exclude conf for windows since we'll be doing the work in the WiX template
if (NOT WIN32)
	install(FILES conf/minifi.properties conf/minifi-log.properties conf/minifi-uid.properties conf/config.yml
			DESTINATION conf
			COMPONENT bin)

	install(DIRECTORY extensions/pythonprocessors/
			DESTINATION minifi-python
			COMPONENT bin)

	install(PROGRAMS bin/minifi.sh
			DESTINATION bin
			COMPONENT bin)
endif()

install(FILES LICENSE README.md NOTICE
        DESTINATION .
        COMPONENT bin)

if(WIN32)
	#preference is to use the exe type so that we have a solution that works well for cross compilation
	#but that leaves the onus up to the developer, so until we can automate some of that build let's enforce
	#the exe here temporarily
	#TODO: Remove this and automate this step.
	#install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/main/minifi.exe
			#DESTINATION bin
			#COMPONENT bin)
else()
	install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/main/minifi
			DESTINATION bin
			COMPONENT bin)
endif() 
        
			
if (WIN32)
	set(CPACK_GENERATOR "WIX")
	set(CPACK_WIX_UPGRADE_GUID "FE29F801-3486-4E9E-AFF9-838C1A5C8D59")
	set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/msi/minifi-logo-ico.ico")
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
if(NOT WIN32)
	set(CPACK_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}")
	set(CPACK_GENERATOR "TGZ")
	set(CPACK_BINARY_TGZ, "ON")
else()
	set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
endif()
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ApacheNiFiMiNiFi")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_RDKAFKA_COMPONENT_INSTALL ON)
set(CPACK_MQTT_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL bin conf minifijni)


include(CPack)
### include modules

if (NOT SKIP_TESTS)
	include(BuildTests)
endif()

## Add KeyValueStorageService tests
registerTest("${TEST_DIR}/keyvalue-tests")

registerTest("${TEST_DIR}/flow-tests")

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
