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

cmake_minimum_required(VERSION 3.24)
cmake_policy(SET CMP0096 NEW) # policy to preserve the leading zeros in PROJECT_VERSION_{MAJOR,MINOR,PATCH,TWEAK}
cmake_policy(SET CMP0065 OLD) # default export policy, required for self-dlopen
cmake_policy(SET CMP0135 NEW) # policy to set the timestamps of extracted contents to the time of extraction

project(nifi-minifi-cpp VERSION 0.99.1)
set(PROJECT_NAME "nifi-minifi-cpp")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Optional build number for linux distribution targets' tar.gz output
set(BUILD_NUMBER "" CACHE STRING "Build number")

# Base Alpine image to be used in the Docker build instead of the default Alpine image
set(DOCKER_BASE_IMAGE "" CACHE STRING "Docker build Alpine base image")
set(DOCKER_CCACHE_DUMP_LOCATION "" CACHE STRING "Directory to dump ccache to after docker build for later reuse")
set(DOCKER_PLATFORMS "" CACHE STRING "Build platforms for docker image build")
set(DOCKER_TAGS "" CACHE STRING "Comma separated tags to override default docker build image:tag pairs with")

include(CMakeDependentOption)
include(CheckIncludeFile)
include(FeatureSummary)
include(ExternalProject)

# Provide custom modules for the project
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(WholeArchive)

include(MiNiFiOptions)

include(DockerConfig)
if (DOCKER_BUILD_ONLY)
    return()
endif()

# Generate the build identifier if one is not provided
if (NOT BUILD_IDENTIFIER)
    string(RANDOM LENGTH 24 BUILD_IDENTIFIER)
    set(BUILD_IDENTIFIER "${BUILD_IDENTIFIER}" CACHE STRING "Build identifier" FORCE)
endif()

message("BUILD_IDENTIFIER is ${BUILD_IDENTIFIER}")

if (WIN32)
    set(LEGAL_COPYRIGHT "Apache License v2.0" CACHE STRING "Used in Windows versioninfo.rc")
    set(COMPANY_NAME "Apache Software Foundation" CACHE STRING "Used in Windows versioninfo.rc")
    set(PRODUCT_NAME "MiNiFi C++" CACHE STRING "Used in Windows versioninfo.rc")
endif()

if (${FORCE_COLORED_OUTPUT})
    message("CMAKE_CXX_COMPILER_ID is ${CMAKE_CXX_COMPILER_ID}")
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options (-fcolor-diagnostics)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        add_compile_options (-fcolor-diagnostics)
    endif()
endif()

if (MINIFI_USE_REAL_ODBC_TEST_DRIVER)
    add_definitions("-DMINIFI_USE_REAL_ODBC_TEST_DRIVER")
endif()

# Use ccache if present
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    message("-- Found ccache: ${CCACHE_FOUND}")
endif(CCACHE_FOUND)

# Check for exec info before we enable the backtrace features.
CHECK_INCLUDE_FILE("execinfo.h" HAS_EXECINFO)
if (ENABLE_OPS AND HAS_EXECINFO AND NOT WIN32)
    add_definitions("-DHAS_EXECINFO=1")
endif()

#### Establish Project Configuration ####
# Enable usage of the VERSION specifier
if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS NOMINMAX)
    add_compile_options(/W3 /utf-8 /bigobj /MP /diagnostics:caret)
    if (CMAKE_GENERATOR STREQUAL "Ninja")
        # these flags are default on msbuild, but not on ninja
        add_compile_options(/MD /GS /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline)
    endif()
endif()

if (NOT PORTABLE)
    if(MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()
endif()

include(CppVersion)
set_cpp_version()

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
if (MINIFI_ADVANCED_ASAN_BUILD)
    set(ASAN_FLAGS "-g -fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ASAN_FLAGS}")
endif()

if (MINIFI_ADVANCED_CODE_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()
endif()

#### Third party dependencies ####

# Define function for passing dependencies
function(append_third_party_passthrough_args OUTPUT EXTERNALPROJECT_CMAKE_ARGS)
    string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH}")
    list(APPEND EXTERNALPROJECT_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    list(APPEND EXTERNALPROJECT_CMAKE_ARGS ${PASSTHROUGH_VARIABLES})
    set(${OUTPUT} ${EXTERNALPROJECT_CMAKE_ARGS} PARENT_SCOPE)
endfunction()

# Find bash executable
find_package(Bash REQUIRED)

# Find patch executable
find_package(Patch REQUIRED)

# Setup passthrough args used by third parties
if (WIN32)
    set(PASSTHROUGH_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
    set(PASSTHROUGH_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /w")
else()
    set(PASSTHROUGH_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
    set(PASSTHROUGH_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
endif()

set(PASSTHROUGH_CMAKE_ARGS -DANDROID_ABI=${ANDROID_ABI}
    -DANDROID_PLATFORM=${ANDROID_PLATFORM}
    -DANDROID_STL=${ANDROID_STL}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DANDROID_NDK=${ANDROID_NDK}
    -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}
    -DCMAKE_C_FLAGS=${PASSTHROUGH_CMAKE_C_FLAGS}
    -DCMAKE_CXX_FLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS}
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
    -DCOMPILE_DEFINITIONS=${COMPILE_DEFINITIONS}
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}
    -G${CMAKE_GENERATOR}
    )

set(MINIFI_CPP_COMPILE_OPTIONS "")
set(MINIFI_CPP_COMPILE_DEFINITIONS "")

if(CUSTOM_MALLOC)
    if (CUSTOM_MALLOC STREQUAL jemalloc)
        include(BundledJemalloc)
        use_bundled_jemalloc(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
        set(CUSTOM_MALLOC_LIB JeMalloc::JeMalloc)
    elseif (CUSTOM_MALLOC STREQUAL mimalloc)
        include(MiMalloc)
        set(CUSTOM_MALLOC_LIB mimalloc)
    elseif (CUSTOM_MALLOC STREQUAL rpmalloc)
        include(RpMalloc)
        set(CUSTOM_MALLOC_LIB rpmalloc)
    else()
        message(FATAL_ERROR "Invalid CUSTOM_MALLOC")
    endif()
else()
    message(VERBOSE "No custom malloc implementation")
endif()

if (ENABLE_BZIP2 AND (ENABLE_LIBARCHIVE OR (ENABLE_ROCKSDB AND NOT WIN32)))
    include(BundledBZip2)
    use_bundled_bzip2(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/bzip2/dummy")
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

# OpenSSL
include(BundledOpenSSL)
use_openssl("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/ssl")

# libsodium
include(BundledLibSodium)
use_bundled_libsodium("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

list(APPEND MINIFI_CPP_COMPILE_DEFINITIONS SODIUM_STATIC=1)

# zlib
include(BundledZLIB)
use_bundled_zlib(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/zlib/dummy")

# cURL
include(BundledLibcURL)
use_bundled_curl(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/curl/dummy")

# spdlog
include(Spdlog)

# yaml-cpp
include(BundledYamlCpp)
use_bundled_yamlcpp(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# concurrentqueue
add_library(concurrentqueue INTERFACE)
target_include_directories(concurrentqueue SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/concurrentqueue")

# RapidJSON
add_library(RapidJSON INTERFACE)
target_include_directories(RapidJSON SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rapidjson-48fbd8cd202ca54031fe799db2ad44ffa8e77c13/include")
target_compile_definitions(RapidJSON INTERFACE RAPIDJSON_HAS_STDSTRING)

include(Coroutines)
enable_coroutines()

# gsl-lite
include(GslLite)

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

# date
include(Date)

# expected-lite
include(ExpectedLite)

# magic_enum
include(MagicEnum)

# Setup warning flags
if(MSVC)
    list(APPEND MINIFI_CPP_COMPILE_OPTIONS /W3)
    if(MINIFI_FAIL_ON_WARNINGS)
        list(APPEND MINIFI_CPP_COMPILE_OPTIONS /WX)
    endif()
else()
    list(APPEND MINIFI_CPP_COMPILE_OPTIONS -Wall -Wextra)
    if(MINIFI_FAIL_ON_WARNINGS)
        list(APPEND MINIFI_CPP_COMPILE_OPTIONS -Werror)
        # -Wrestrict may cause build failure in GCC 12  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104336
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            list(APPEND MINIFI_CPP_COMPILE_OPTIONS -Wno-error=restrict)
        endif()
    endif()
endif()

function(add_minifi_library LIBRARY_NAME LINK_TYPE)
    add_library(${LIBRARY_NAME} ${LINK_TYPE} ${ARGN})
    target_compile_options(${LIBRARY_NAME} PRIVATE ${MINIFI_CPP_COMPILE_OPTIONS})
    target_compile_definitions(${LIBRARY_NAME} PRIVATE ${MINIFI_CPP_COMPILE_DEFINITIONS})
endfunction()

function(add_minifi_executable EXECUTABLE_NAME)
    add_executable(${EXECUTABLE_NAME} ${ARGN})
    target_compile_options(${EXECUTABLE_NAME} PRIVATE ${MINIFI_CPP_COMPILE_OPTIONS})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ${MINIFI_CPP_COMPILE_DEFINITIONS})
endfunction()

#### Extensions ####
SET(TEST_DIR ${CMAKE_SOURCE_DIR}/libminifi/test)
include(Extensions)

add_subdirectory(core)
add_subdirectory(utils)
add_subdirectory(extension-utils)
add_subdirectory(libminifi)

if (ENABLE_ALL OR ENABLE_AZURE)
    include(BundledLibXml2)
    use_bundled_libxml2(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/libxml2/dummy")
endif()

if (ENABLE_ALL OR ENABLE_PROMETHEUS OR ENABLE_CIVET)
    include(CivetWeb)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/civetweb/dummy")
endif()

## Add extensions
file(GLOB extension-directories "extensions/*")
foreach(extension-dir ${extension-directories})
    if (IS_DIRECTORY ${extension-dir} AND EXISTS ${extension-dir}/CMakeLists.txt)
        add_subdirectory(${extension-dir})
    endif()
endforeach()

## NOW WE CAN ADD LIBRARIES AND EXTENSIONS TO MAIN
add_subdirectory(minifi_main)

if (ENABLE_ENCRYPT_CONFIG)
    add_subdirectory(encrypt-config)
endif()

if (ENABLE_CONTROLLER)
    add_subdirectory(controller)
endif()


get_property(selected_extensions GLOBAL PROPERTY EXTENSION-OPTIONS)

if (WIN32)
    # Get the latest abbreviated commit hash of the working branch
    execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE BUILD_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ECHO STDOUT
        ECHO_OUTPUT_VARIABLE)

    execute_process(COMMAND powershell
        "(New-Object -ComObject Scripting.FileSystemObject).GetFolder('${CMAKE_CURRENT_SOURCE_DIR}').ShortPath"
        OUTPUT_VARIABLE CMAKE_CURRENT_SOURCE_DIR_SHORT_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ECHO STDOUT
        ECHO_OUTPUT_VARIABLE)

    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR_SHORT_PATH}/generateVersion.bat
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/libminifi"
        "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CXX_COMPILER_VERSION}"
        "${CMAKE_CXX_FLAGS};${MINIFI_CPP_COMPILE_OPTIONS};${MINIFI_CPP_COMPILE_DEFINITIONS}"
        \"${selected_extensions}\"
        "${BUILD_IDENTIFIER}"
        "${BUILD_REV}"
        COMMAND_ECHO STDOUT)
else()
    execute_process(COMMAND
        "${CMAKE_CURRENT_SOURCE_DIR}/generateVersion.sh"
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/libminifi
        "${CMAKE_CXX_COMPILER}"
        "${CMAKE_CXX_COMPILER_VERSION}"
        "${CMAKE_CXX_FLAGS};${MINIFI_CPP_COMPILE_OPTIONS};${MINIFI_CPP_COMPILE_DEFINITIONS}"
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
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(GLOB markdown_docs "${CMAKE_CURRENT_SOURCE_DIR}/*.md")
    foreach(mddocfile ${markdown_docs})
        file(COPY "${mddocfile}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()

    # Determine the path of the VC Redistributable Merge Modules
    if (DEFINED ENV{VCToolsRedistDir})
        # Just get the redist dir that has been set by the build environment
        set(VCRUNTIME_REDIST_DIR $ENV{VCToolsRedistDir})
    else()
        # Try to fall back to a redist dir relative to the MSVC compiler
        string(REGEX REPLACE "/VC/Tools/MSVC/([0-9]+\\.[0-9]+).*" "/VC/Redist/MSVC/\\1" VCRUNTIME_REDIST_BASE_DIR ${CMAKE_C_COMPILER})
        file(GLOB VCRUNTIME_REDIST_VERSIONS "${VCRUNTIME_REDIST_BASE_DIR}*")
        if (NOT VCRUNTIME_REDIST_VERSIONS)
            message(FATAL_ERROR "Could not find the VC Redistributable Merge Modules. Please set VCRUNTIME_X86_MERGEMODULE_PATH and VCRUNTIME_X64_MERGEMODULE_PATH manually!")
        endif()
        # Sort the directories in descending order and take the first one - it should be the latest version
        list(SORT VCRUNTIME_REDIST_VERSIONS)
        list(REVERSE VCRUNTIME_REDIST_VERSIONS)
        list(GET VCRUNTIME_REDIST_VERSIONS 0 VCRUNTIME_REDIST_DIR)
    endif()
    message("Using redist directory: ${VCRUNTIME_REDIST_DIR}")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(BUILD_PLATFORM "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(BUILD_PLATFORM "x86")
    else()
        message(FATAL_ERROR "Could not determine architecture, CMAKE_SIZEOF_VOID_P is unexpected: ${CMAKE_SIZEOF_VOID_P}")
    endif()

    set(WIX_EXTRA_FEATURES "")
    set(WIX_EXTRA_COMPONENTS "")
    if (MINIFI_INCLUDE_UCRT_DLLS)
        message("Creating installer with Universal C Runtime DLLs")
        set(UCRT_DIR_NAT "$ENV{WindowsSdkDir}Redist\\ucrt\\DLLs\\${BUILD_PLATFORM}")
        file(TO_CMAKE_PATH "${UCRT_DIR_NAT}" UCRT_DIR)
        if (NOT EXISTS "${UCRT_DIR}")
            set(UCRT_DIR_NAT "$ENV{WindowsSdkDir}Redist\\$ENV{WindowsSDKVersion}ucrt\\DLLs\\${BUILD_PLATFORM}")
            file(TO_CMAKE_PATH "${UCRT_DIR_NAT}" UCRT_DIR)
        endif()
        if (NOT EXISTS "${UCRT_DIR}")
            message(FATAL_ERROR "Couldn't find UCRT")
        else()
            message("Using UCRT from ${UCRT_DIR}")
            file(GLOB UCRT_DLLS "${UCRT_DIR}/*.dll")
            file(COPY ${UCRT_DLLS} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/ucrt")
            install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ucrt/"
                    DESTINATION bin
                    COMPONENT bin
                    )
        endif()
    endif()
    if(MINIFI_INCLUDE_VC_REDIST_MERGE_MODULES)
        message("Creating installer with merge modules for Visual C++ Redistributable")
        if(MINIFI_INCLUDE_VC_REDIST_DLLS)
            message(WARNING "MINIFI_INCLUDE_VC_REDIST_MERGE_MODULES and MINIFI_INCLUDE_VC_REDIST_DLLS are both set, only MINIFI_INCLUDE_VC_REDIST_MERGE_MODULES will be used")
        endif()
        file(GLOB VCRUNTIME_X86_MERGEMODULES "${VCRUNTIME_REDIST_DIR}/MergeModules/Microsoft_VC*_CRT_x86.msm")
        file(GLOB VCRUNTIME_X64_MERGEMODULES "${VCRUNTIME_REDIST_DIR}/MergeModules/Microsoft_VC*_CRT_x64.msm")
        if (NOT VCRUNTIME_X86_MERGEMODULES OR NOT VCRUNTIME_X64_MERGEMODULES)
            message(FATAL_ERROR "Could not find the VC Redistributable Merge Modules. Please set VCRUNTIME_X86_MERGEMODULE_PATH and VCRUNTIME_X64_MERGEMODULE_PATH manually!")
        else()
            list(GET VCRUNTIME_X86_MERGEMODULES 0 VCRUNTIME_X86_MERGEMODULE_PATH)
            list(GET VCRUNTIME_X64_MERGEMODULES 0 VCRUNTIME_X64_MERGEMODULE_PATH)
        endif()

        if (BUILD_PLATFORM STREQUAL "x64")
            message("Using ${VCRUNTIME_X64_MERGEMODULE_PATH} VC Redistributable Merge Module")
            configure_file("msi/x64.wsi" "msi/x64.wsi" @ONLY)
            list(APPEND CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/msi/x64.wsi")
        else()
            message("Using ${VCRUNTIME_X86_MERGEMODULE_PATH} VC Redistributable Merge Module")
            configure_file("msi/x86.wsi" "msi/x86.wsi" @ONLY)
            list(APPEND CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/msi/x86.wsi")
        endif()

        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/msi/MergeModulesFeature.xml" WIX_EXTRA_FEATURES)
    elseif(MINIFI_INCLUDE_VC_REDIST_DLLS)
        message("Creating installer with Visual C++ Redistributable DLLs")
        file(GLOB VC_RUNTIME_X86_REDIST_CRT_DIR_LIST LIST_DIRECTORIES true "${VCRUNTIME_REDIST_DIR}/x86/Microsoft.VC*.CRT")
        file(GLOB VC_RUNTIME_X64_REDIST_CRT_DIR_LIST LIST_DIRECTORIES true "${VCRUNTIME_REDIST_DIR}/x64/Microsoft.VC*.CRT")
        if (NOT VC_RUNTIME_X86_REDIST_CRT_DIR_LIST OR NOT VC_RUNTIME_X64_REDIST_CRT_DIR_LIST)
            message(FATAL_ERROR "Could not find the VC Redistributable. Please set VCRUNTIME_X86_REDIST_CRT_DIR and VCRUNTIME_X64_REDIST_CRT_DIR manually!")
        else()
            list(GET VC_RUNTIME_X86_REDIST_CRT_DIR_LIST 0 VCRUNTIME_X86_REDIST_CRT_DIR)
            list(GET VC_RUNTIME_X64_REDIST_CRT_DIR_LIST 0 VCRUNTIME_X64_REDIST_CRT_DIR)
        endif()

        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            message("Using ${VCRUNTIME_X64_REDIST_CRT_DIR} VC Redistributables")
            file(GLOB VCRUNTIME_X64_REDIST_CRT_FILES "${VCRUNTIME_X64_REDIST_CRT_DIR}/*.dll")
            file(COPY ${VCRUNTIME_X64_REDIST_CRT_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            message("Using ${VCRUNTIME_X86_REDIST_CRT_DIR} VC Redistributables")
            file(GLOB VCRUNTIME_X86_REDIST_CRT_FILES "${VCRUNTIME_X86_REDIST_CRT_DIR}/*.dll")
            file(COPY ${VCRUNTIME_X86_REDIST_CRT_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/redist")
        else()
            message(FATAL_ERROR "Could not determine architecture, CMAKE_SIZEOF_VOID_P is unexpected: ${CMAKE_SIZEOF_VOID_P}")
        endif()
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/msi/VSRedistributablesComponent.xml" WIX_EXTRA_COMPONENTS)
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/msi/VSRedistributablesFeature.xml" WIX_EXTRA_FEATURES)
    else()
        message("Creating installer without UCRT or VC Redistributable")
    endif()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWin.wsi.in" "${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWin.wsi")
    set(CPACK_WIX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/msi/WixWin.wsi")
else()
    set(CPACK_SOURCE_GENERATOR "TGZ")
endif(WIN32)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}-source")
set(CPACK_SOURCE_IGNORE_FILES "/docs/generated/;${CMAKE_SOURCE_DIR}/build/;~$;${CPACK_SOURCE_IGNORE_FILES};${CMAKE_SOURCE_DIR}/.git/;${CMAKE_SOURCE_DIR}/.idea/;${CMAKE_SOURCE_DIR}/cmake-build-debug/")

# Generate binary assembly. Exclude conf for windows since we'll be doing the work in the WiX template
if (NOT WIN32)
    install(FILES conf/minifi.properties conf/minifi-log.properties conf/minifi-uid.properties conf/config.yml
        DESTINATION conf
        COMPONENT bin)

    install(DIRECTORY extensions/python/pythonprocessors/
        DESTINATION minifi-python
        COMPONENT bin)

    install(DIRECTORY extensions/python/pythonprocessor-examples/
        DESTINATION minifi-python-examples
        COMPONENT bin)

    install(PROGRAMS bin/minifi.sh
            DESTINATION bin
            COMPONENT bin)

    if (APPLE)
        install(FILES bin/minifi.plist
            DESTINATION bin
            COMPONENT bin)
    else()
        install(FILES bin/minifi.service
                DESTINATION bin
                COMPONENT bin)
    endif()
endif()

install(FILES LICENSE NOTICE
    DESTINATION .
    COMPONENT bin)
file(GLOB markdown_docs "*.md")
install(FILES ${markdown_docs}
    DESTINATION .
    COMPONENT bin)

include(CPackComponent)

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
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
if(NOT WIN32)
    set(CPACK_PACKAGE_FILE_NAME "${ASSEMBLY_BASE_NAME}")
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_BINARY_TGZ, "ON")
else()
    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
endif()
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ApacheNiFiMiNiFi")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING "ALL_COMPONENTS_IN_ONE")


# !WARNING! we need to manually add to CPACK_COMPONENTS_ALL otherwise
# all components specified in "install" directives are added
# (including thirdparties like range-v3)

list(APPEND CPACK_COMPONENTS_ALL bin)
cpack_add_component(bin DISPLAY_NAME "MiNiFi C++ executables" REQUIRED)
if(NOT WIN32)
    list(APPEND CPACK_COMPONENTS_ALL conf)
    cpack_add_component(conf DISPLAY_NAME "Default configuration files" REQUIRED)
endif()

cpack_add_component_group(extensions DISPLAY_NAME "Extensions" EXPANDED)
set(EXTENSIONS_ENABLED_BY_DEFAULT (
    minifi-aws
    minifi-azure
    minifi-civet-extensions
    minifi-elasticsearch
    minifi-expression-language-extensions
    minifi-gcp
    minifi-grafana-loki
    minifi-archive-extensions
    minifi-mqtt-extensions
    minifi-rdkafka-extensions
    minifi-pdh
    minifi-prometheus
    minifi-rocksdb-repos
    minifi-smb
    minifi-splunk
    minifi-sql
    minifi-standard-processors
    minifi-wel
))
foreach(extension ${selected_extensions})
    get_component_name(${extension} component-name)
    list(APPEND CPACK_COMPONENTS_ALL ${component-name})
    if(${extension} IN_LIST EXTENSIONS_ENABLED_BY_DEFAULT)
        set(maybe_disabled "")
    else()
        set(maybe_disabled DISABLED)
    endif()
    cpack_add_component(${component-name} DISPLAY_NAME ${extension} ${maybe_disabled} GROUP extensions DEPENDS bin)
endforeach()
if ("python_script_extension" IN_LIST CPACK_COMPONENTS_ALL)
    if (WIN32)
        set(CPACK_WIX_CANDLE_EXTRA_FLAGS "-dINCLUDE_PYTHON_PROCESSORS=yes")
    else()
        list(APPEND CPACK_COMPONENTS_ALL minifi_python_native_module)
        cpack_add_component(minifi_python_native_module
                DISPLAY_NAME "MiNiFi Python native module"
                FILES "${CMAKE_BINARY_DIR}/bin/minifi_native.so"
                GROUP extensions
                DEPENDS bin)
    endif()
endif()

include(CPack)
### include modules

if (NOT SKIP_TESTS)
    include(BuildTests)

    add_subdirectory("${TEST_DIR}/libtest")

    add_subdirectory("${TEST_DIR}/unit")

    add_subdirectory("${TEST_DIR}/integration")

    ## Add KeyValueStorageService tests
    add_subdirectory("${TEST_DIR}/keyvalue-tests")

    add_subdirectory("${TEST_DIR}/flow-tests")

    add_subdirectory("${TEST_DIR}/schema-tests")

    if (ENABLE_ROCKSDB AND ENABLE_LIBARCHIVE)
        add_subdirectory("${TEST_DIR}/persistence-tests")
    endif()

    add_subdirectory("minifi_main/tests")

    add_subdirectory("encrypt-config/tests")

    add_subdirectory("controller/tests")
endif()

include(BuildDocs)


# Create a custom build target that will run the linter.
# Directories have their separate linter targets to be able to use better parallelization
get_property(extensions GLOBAL PROPERTY EXTENSION-LINTERS)
set(root_linted_dirs libminifi/include libminifi/src libminifi/test encrypt-config controller)
list(TRANSFORM root_linted_dirs PREPEND ${CMAKE_SOURCE_DIR}/)

set(linted_dir_counter 1)
set(root_linter_target_names "")

foreach(linted_dir ${root_linted_dirs})
    set(linter_target_name "root-linter-${linted_dir_counter}")
    list(APPEND root_linter_target_names ${linter_target_name})
    add_custom_target("${linter_target_name}"
        COMMAND python3 ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.py -q -i ${linted_dir}
    )
    set_target_properties(${linter_target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    math(EXPR linted_dir_counter "${linted_dir_counter}+1")
endforeach()

# Main linter target that depends on every other
add_custom_target(linter)
add_dependencies(linter ${root_linter_target_names} ${extensions})
set_target_properties(linter PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)


if(NOT WIN32)
add_custom_target(shellcheck
    COMMAND ${CMAKE_SOURCE_DIR}/run_shellcheck.sh ${CMAKE_SOURCE_DIR})

add_custom_target(flake8
    COMMAND ${CMAKE_SOURCE_DIR}/run_flake8.sh ${CMAKE_SOURCE_DIR})
endif(NOT WIN32)

# Custom target to download and run Apache Release Audit Tool (RAT)
add_custom_target(
    apache-rat
    ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/RunApacheRAT.cmake
    COMMAND java -jar ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat/apache-rat-0.13/apache-rat-0.13.jar -E ${CMAKE_SOURCE_DIR}/thirdparty/apache-rat/.rat-excludes -d ${CMAKE_SOURCE_DIR} | grep -B 1 -A 15 Summary )

feature_summary(WHAT ALL FILENAME ${CMAKE_BINARY_DIR}/all.log)

if (MINIFI_ADVANCED_CODE_COVERAGE)
    set(GCOVR_ADDITIONAL_ARGS --gcov-ignore-parse-errors=negative_hits.warn --gcov-ignore-errors=no_working_dir_found)
    setup_target_for_coverage_gcovr_html(
        NAME coverage
        EXCLUDE "build/*" "cmake/*" "minifi_main/*" "thirdparty/*" "libminifi/test/*" "encrypt-config/tests/*" "extensions/aws/tests/*"
            "extensions/civetweb/tests/*" "extensions/elasticsearch/tests/*" "extensions/expression-language/tests/*" "extensions/gcp/tests/*" "extensions/grafana-loki/tests/*"
            "extensions/kubernetes/tests/*" "extensions/librdkafka/tests/*" "extensions/lua/tests/*" "extensions/mqtt/tests/*" "extensions/opencv/tests/*" "extensions/procfs/tests/*" "extensions/prometheus/tests/*"
            "extensions/script/tests/*" "extensions/sftp/tests/*" "extensions/splunk/tests/*" "extensions/standard-processors/tests/*" "extensions/systemd/tests/*" "extensions/test-processors/*"
            "controller/MiNiFiController.cpp"
    )
endif()
