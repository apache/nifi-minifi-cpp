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
include(CMakeDependentOption)

set(MINIFI_OPTIONS "")
function(add_minifi_option OPTION_NAME OPTION_DESCRIPTION OPTION_VALUE)
    option(${OPTION_NAME} ${OPTION_DESCRIPTION} ${OPTION_VALUE})
    list(APPEND MINIFI_OPTIONS ${OPTION_NAME})
    set(MINIFI_OPTIONS ${MINIFI_OPTIONS} PARENT_SCOPE)
endfunction()

function(add_minifi_multi_option OPTION_NAME OPTION_DESCRIPTION OPTION_VALUES DEFAULT_VALUE)
    set(${OPTION_NAME} ${DEFAULT_VALUE} CACHE STRING ${OPTION_DESCRIPTION})
    set_property(CACHE ${OPTION_NAME} PROPERTY STRINGS ${OPTION_VALUES})
    list(APPEND MINIFI_OPTIONS ${OPTION_NAME})
    set(MINIFI_OPTIONS ${MINIFI_OPTIONS} PARENT_SCOPE)
endfunction()

function(add_minifi_dependent_option OPTION_NAME OPTION_DESCRIPTION OPTION_VALUE DEPENDS FORCE)
    cmake_dependent_option(${OPTION_NAME} ${OPTION_DESCRIPTION} ${OPTION_VALUE} ${DEPENDS} ${FORCE})
    list(APPEND MINIFI_OPTIONS ${OPTION_NAME})
    set(MINIFI_OPTIONS ${MINIFI_OPTIONS} PARENT_SCOPE)
endfunction()

function(set_minifi_cache_variable VARIABLE_NAME VARIABLE_VALUE DOCSTRING)
    set(${VARIABLE_NAME} ${VARIABLE_VALUE} CACHE STRING ${DOCSTRING})
    list(APPEND MINIFI_OPTIONS ${VARIABLE_NAME})
    set(MINIFI_OPTIONS ${MINIFI_OPTIONS} PARENT_SCOPE)
endfunction()

add_minifi_option(CI_BUILD "Build is used for CI." OFF)
add_minifi_option(SKIP_TESTS "Skips building all tests." OFF)
add_minifi_option(DOCKER_BUILD_ONLY "Disables all targets except docker build scripts. Ideal for systems without an up-to-date compiler." OFF)
add_minifi_option(DOCKER_SKIP_TESTS "Skip building tests in docker image targets." ON)
add_minifi_option(DOCKER_PUSH "Push created images to the specified tags" OFF)

add_minifi_option(PORTABLE "Instructs the compiler to remove architecture specific optimizations" ON)
add_minifi_option(USE_SHARED_LIBS "Builds using shared libraries" ON)
add_minifi_dependent_option(STATIC_BUILD "Attempts to statically link as many dependencies as possible." ON "NOT USE_SHARED_LIBS" OFF)
add_minifi_option(LIBC_STATIC "Instructs the build system to statically link libstdc++ and glibc into minifiexe. Experiemental" OFF)
add_minifi_option(ENABLE_OPS "Enable Operations/zlib Tools" ON)
add_minifi_option(ENABLE_OPC "Instructs the build system to enable the OPC extension" ON)
add_minifi_option(BUILD_SHARED_LIBS "Build yaml cpp shared lib" OFF)

add_minifi_option(BUILD_ROCKSDB "Instructs the build system to use RocksDB from the third party directory" ON)

add_minifi_option(MINIFI_FAIL_ON_WARNINGS "Treat warnings as errors" OFF)
add_minifi_option(MINIFI_USE_REAL_ODBC_TEST_DRIVER "Use SQLite ODBC driver in SQL extenstion unit tests instead of a mock database" OFF)
# This is needed for ninja:
# By default, neither Clang or GCC will add ANSI-formatted colors to your output if they detect
# the output medium is not a terminal. This means no coloring when using a generator
# different than "GNU Makefiles".
add_minifi_option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)
add_minifi_option(AWS_ENABLE_UNITY_BUILD "If enabled, AWS SDK libraries will be built as a single, generated .cpp file. \
    This can significantly reduce static library size as well as speed up a single compilation time, but it is regenerated \
    and recompiled in every iterative build instance. Turn off to avoid recompilation." ON)

add_minifi_dependent_option(MINIFI_ADVANCED_ASAN_BUILD "Uses AddressSanitizer to instrument the code" OFF "NOT WIN32" OFF)
add_minifi_dependent_option(MINIFI_ADVANCED_CODE_COVERAGE "Use coverage build options and enable coverage build target" OFF "NOT WIN32" OFF)

if (WIN32)
    add_minifi_option(MINIFI_INCLUDE_VC_REDIST_MERGE_MODULES "Include merge modules for Visual C++ Redistributable with the MSI generated by CPack. The resulting MSI is not distributable under Apache 2.0." OFF)
    add_minifi_option(MINIFI_INCLUDE_VC_REDIST_DLLS "Include Visual C++ Redistributable DLLs with the MSI generated by CPack. The resulting MSI is not distributable under Apache 2.0." OFF)
    add_minifi_option(MINIFI_INCLUDE_UCRT_DLLS "Redistribute Universal C Runtime DLLs with the MSI generated by CPack. The resulting MSI is not distributable under Apache 2.0." OFF)
    add_minifi_option(ENABLE_WEL "Enables the suite of Windows Event Log extensions." ON)
    add_minifi_option(ENABLE_PDH "Enables PDH support." ON)
    add_minifi_option(ENABLE_SMB "Enables SMB support." ON)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_minifi_option(ENABLE_SYSTEMD "Enables the systemd extension." ON)
endif()

add_minifi_option(ENABLE_ALL "Enables all extensions" OFF)
add_minifi_option(ENABLE_EXPRESSION_LANGUAGE "Enables expression language." ON)
add_minifi_option(ENABLE_CIVET "Enables CivetWeb components." ON)
add_minifi_option(ENABLE_ROCKSDB "Enables the RocksDB extension." ON)
add_minifi_option(ENABLE_LIBARCHIVE "Enables the lib archive extensions." ON)
add_minifi_option(ENABLE_LZMA "Enables the liblzma build" ON)
add_minifi_option(ENABLE_BZIP2 "Enables the bzip2 build" ON)
add_minifi_option(ENABLE_SQL "Enables the SQL Suite of Tools." ON)
add_minifi_option(ENABLE_MQTT "Enables the mqtt extension." ON)
add_minifi_option(ENABLE_KAFKA "Enables the kafka extension." ON)
add_minifi_option(ENABLE_LUA_SCRIPTING "Enables lua scripting" ON)
add_minifi_option(ENABLE_PYTHON_SCRIPTING "Enables python scripting" ON)
add_minifi_option(ENABLE_AWS "Enables AWS support." ON)
add_minifi_option(ENABLE_OPENCV "Enables the OpenCV extensions." OFF)
add_minifi_option(ENABLE_BUSTACHE "Enables Bustache (ApplyTemplate) support." OFF)
add_minifi_option(ENABLE_SFTP "Enables SFTP support." OFF)
add_minifi_option(ENABLE_AZURE "Enables Azure support." ON)
add_minifi_option(ENABLE_ENCRYPT_CONFIG "Enables build of encrypt-config binary." ON)
add_minifi_option(ENABLE_SPLUNK "Enable Splunk support" ON)
add_minifi_option(ENABLE_ELASTICSEARCH "Enable Elasticsearch support" ON)
add_minifi_option(ENABLE_GCP "Enable Google Cloud support" ON)
add_minifi_option(ENABLE_KUBERNETES "Enables the Kubernetes extensions." ON)
add_minifi_option(ENABLE_TEST_PROCESSORS "Enables test processors" OFF)
add_minifi_option(ENABLE_PROMETHEUS "Enables Prometheus support." ON)
add_minifi_option(ENABLE_GRAFANA_LOKI "Enable Grafana Loki support" OFF)
add_minifi_option(ENABLE_GRPC_FOR_LOKI "Enable gRPC for Grafana Loki extension" ON)
add_minifi_option(ENABLE_COUCHBASE "Enable Couchbase support" OFF)
add_minifi_option(ENABLE_EXECUTE_PROCESS "Enable ExecuteProcess processor" OFF)
add_minifi_option(ENABLE_CONTROLLER "Enables the build of MiNiFi controller binary." ON)
add_minifi_option(ENABLE_LLAMACPP "Enables LlamaCpp support." ON)

set_minifi_cache_variable(CUSTOM_MALLOC OFF "Overwrite malloc implementation.")
set_property(CACHE CUSTOM_MALLOC PROPERTY STRINGS "jemalloc" "mimalloc" "rpmalloc" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_minifi_option(ENABLE_PROCFS "Enables the procfs extension." ON)
endif()

## Each Option provides a list of possible values

# Option: STRICT_GSL_CHECKS
# AUDIT: Enable all checks, including gsl_ExpectsAudit() and gsl_EnsuresAudit()
# ON: Enable all checks, excluding gsl_ExpectsAudit() and gsl_EnsuresAudit() (GSL default)
# DEBUG_ONLY: Like ON in the Debug configuration, OFF in others (MiNiFi C++ default)
# OFF: Throw on contract checking and assertion failures instead of calling std::terminate()
set(STRICT_GSL_CHECKS "DEBUG_ONLY" CACHE STRING "Contract checking and assertion failures call terminate")
list(APPEND STRICT_GSL_CHECKS_Values AUDIT ON DEBUG_ONLY OFF)
set_property(CACHE STRICT_GSL_CHECKS PROPERTY STRINGS ${STRICT_GSL_CHECKS_Values})

# BUILD: Fetch and build from source using CMake FetchContent or ExternalProject
# SYSTEM: Use find_package to use the system version
# CONAN: Use Conan packages
add_minifi_multi_option(MINIFI_LIBCURL_SOURCE "Retrieves LibCURL from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_OPENSSL_SOURCE "Retrieves OpenSSL from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_ZLIB_SOURCE "Retrieves ZLib from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_ROCKSDB_SOURCE "Retrieves RocksDB from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_ZSTD_SOURCE "Retrieves Zstd from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_BZIP2_SOURCE "Retrieves BZip2 from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_CIVETWEB_SOURCE "Retrieves CivetWeb from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_LIBXML2_SOURCE "Retrieves LibXml2 from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_FMT_SOURCE "Retrieves Fmt from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_SPDLOG_SOURCE "Retrieves Spdlog from provided source" "BUILD;SYSTEM;CONAN" "BUILD")
add_minifi_multi_option(MINIFI_CATCH2_SOURCE "Retrieves Catch2 from provided source" "BUILD;SYSTEM;CONAN" "BUILD")

# Docker options

function(get_minifi_docker_options RET_VALUE)
    set(MINIFI_DOCKER_OPTIONS_STR ${MINIFI_EXTERNAL_DOCKER_OPTIONS_STR})
    foreach(MINIFI_OPTION ${MINIFI_OPTIONS})
        if (MINIFI_OPTION STREQUAL "SKIP_TESTS" OR MINIFI_OPTION STREQUAL "DOCKER_BUILD_ONLY" OR MINIFI_OPTION STREQUAL "DOCKER_SKIP_TESTS" OR MINIFI_OPTION STREQUAL "DOCKER_PUSH")
            continue()
        endif()
        set(MINIFI_DOCKER_OPTIONS_STR "${MINIFI_DOCKER_OPTIONS_STR} -D${MINIFI_OPTION}=${${MINIFI_OPTION}}")
    endforeach()
    set(${RET_VALUE} ${MINIFI_DOCKER_OPTIONS_STR} PARENT_SCOPE)
endfunction()


function(print_minifi_options)
    foreach(option ${MINIFI_OPTIONS})
        message(STATUS "  MiNiFi Option: ${option}: ${${option}}")
    endforeach()
endfunction()

set(MINIFI_DOCKER_OPTIONS_STR "")
get_minifi_docker_options(MINIFI_DOCKER_OPTIONS_STR)
list(SORT MINIFI_OPTIONS)
print_minifi_options()
