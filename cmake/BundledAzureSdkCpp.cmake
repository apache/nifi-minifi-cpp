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

function(use_bundled_libazure SOURCE_DIR BINARY_DIR)
    set(PATCH_FILE "${SOURCE_DIR}/thirdparty/azure-sdk-cpp/azure-sdk-cpp-remove-samples.patch")
    set(PC ${Bash_EXECUTABLE} -c "set -x && \
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE}\")")
    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
        set(PREFIX "")
        set(AZURE_CORE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${CMAKE_BUILD_TYPE}/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${CMAKE_BUILD_TYPE}/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${CMAKE_BUILD_TYPE}/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${CMAKE_BUILD_TYPE}/${PREFIX}azure-identity.${SUFFIX}")
        set(AZURE_STORAGE_FILES_DATALAKE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-files-datalake/${CMAKE_BUILD_TYPE}/${PREFIX}azure-storage-files-datalake.${SUFFIX}")
    else()
        set(SUFFIX "a")
        set(PREFIX "lib")
        set(AZURE_CORE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${PREFIX}azure-identity.${SUFFIX}")
        set(AZURE_STORAGE_FILES_DATALAKE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-files-datalake/${PREFIX}azure-storage-files-datalake.${SUFFIX}")
    endif()

    set(AZURESDK_LIBRARIES_LIST
            "${AZURE_CORE_LIB}"
            "${AZURE_STORAGE_COMMON_LIB}"
            "${AZURE_STORAGE_BLOBS_LIB}"
            "${AZURE_IDENTITY_LIB}"
            "${AZURE_STORAGE_FILES_DATALAKE_LIB}")

    set(AZURE_SDK_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
        -DWARNINGS_AS_ERRORS=OFF)
    append_third_party_passthrough_args(AZURE_SDK_CMAKE_ARGS "${AZURE_SDK_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            azure-sdk-cpp-external
            GIT_REPOSITORY "https://github.com/Azure/azure-sdk-for-cpp.git"
            GIT_TAG "azure-storage-files-datalake_12.2.0"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src"
            BUILD_BYPRODUCTS "${AZURESDK_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
            STEP_TARGETS build
            CMAKE_ARGS ${AZURE_SDK_CMAKE_ARGS}
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            PATCH_COMMAND ${PC}
    )

    # Set dependencies
    add_dependencies(azure-sdk-cpp-external-build CURL::libcurl LibXml2::LibXml2 OpenSSL::Crypto OpenSSL::SSL)

    # Set variables
    set(LIBAZURE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAZURE_INCLUDE_DIRS
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-files-datalake/inc/"
            CACHE STRING "" FORCE)
    set(LIBAZURE_LIBRARIES ${AZURESDK_LIBRARIES_LIST} CACHE STRING "" FORCE)

    # Create imported targets
    FOREACH(LIBAZURE_INCLUDE_DIR ${LIBAZURE_INCLUDE_DIRS})
        file(MAKE_DIRECTORY ${LIBAZURE_INCLUDE_DIR})
    ENDFOREACH(LIBAZURE_INCLUDE_DIR)

    add_library(AZURE::azure-core STATIC IMPORTED)
    set_target_properties(AZURE::azure-core PROPERTIES IMPORTED_LOCATION "${AZURE_CORE_LIB}")
    add_dependencies(AZURE::azure-core azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-core INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-core INTERFACE CURL::libcurl OpenSSL::Crypto OpenSSL::SSL)
    if (WIN32)
        target_link_libraries(AZURE::azure-core INTERFACE winhttp.lib WebServices.lib)
    endif()

    add_library(AZURE::azure-identity STATIC IMPORTED)
    set_target_properties(AZURE::azure-identity PROPERTIES IMPORTED_LOCATION "${AZURE_IDENTITY_LIB}")
    add_dependencies(AZURE::azure-identity azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-identity INTERFACE ${LIBAZURE_INCLUDE_DIRS})

    add_library(AZURE::azure-storage-common STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-common PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_COMMON_LIB}")
    add_dependencies(AZURE::azure-storage-common azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-storage-common INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-storage-common INTERFACE LibXml2::LibXml2)

    add_library(AZURE::azure-storage-blobs STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-blobs PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_BLOBS_LIB}")
    add_dependencies(AZURE::azure-storage-blobs azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-storage-blobs INTERFACE ${LIBAZURE_INCLUDE_DIRS})

    add_library(AZURE::azure-storage-files-datalake STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-files-datalake PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_FILES_DATALAKE_LIB}")
    add_dependencies(AZURE::azure-storage-files-datalake azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-storage-files-datalake INTERFACE ${LIBAZURE_INCLUDE_DIRS})
endfunction(use_bundled_libazure)
