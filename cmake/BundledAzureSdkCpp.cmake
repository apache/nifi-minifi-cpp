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
    set(PATCH_FILE1 "${SOURCE_DIR}/thirdparty/azure-sdk-cpp-for-cpp/azure-sdk-for-cpp-old-compiler.patch")
    set(PATCH_FILE2 "${SOURCE_DIR}/thirdparty/azure-sdk-cpp-for-cpp/fix-illegal-qualified-name-in-member.patch")
    set(PC bash -c "set -x &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE1}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE2}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE2}\") ")


    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
        set(PREFIX "")
        set(AZURE_CORE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${CMAKE_BUILD_TYPE}/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${CMAKE_BUILD_TYPE}/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${CMAKE_BUILD_TYPE}/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${CMAKE_BUILD_TYPE}/${PREFIX}azure-identity.${SUFFIX}")
    else()
        set(SUFFIX "a")
        set(PREFIX "lib")
        set(AZURE_CORE_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${PREFIX}azure-identity.${SUFFIX}")
    endif()

    set(AZURESDK_LIBRARIES_LIST
            "${AZURE_CORE_LIB}"
            "${AZURE_STORAGE_COMMON_LIB}"
            "${AZURE_STORAGE_BLOBS_LIB}"
            "${AZURE_IDENTITY_LIB}")

    set(AZURE_SDK_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
        -DWARNINGS_AS_ERRORS=OFF)
    append_third_party_passthrough_args(AZURE_SDK_CMAKE_ARGS "${AZURE_SDK_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            azure-sdk-cpp-external
            GIT_REPOSITORY "https://github.com/Azure/azure-sdk-for-cpp.git"
            GIT_TAG "azure-storage-blobs_12.0.0-beta.7"
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
    add_dependencies(azure-sdk-cpp-external-build CURL::libcurl LibXml2::LibXml2 OpenSSL::Crypto OpenSSL::SSL nlohmann_json::nlohmann_json)

    # Set variables
    set(LIBAZURE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAZURE_INCLUDE_DIRS
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/inc/"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/inc/"
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
    target_link_libraries(AZURE::azure-core INTERFACE LibXml2::LibXml2 CURL::libcurl OpenSSL::Crypto OpenSSL::SSL Threads::Threads nlohmann_json::nlohmann_json)
    if (APPLE)
        target_link_libraries(AZURE::azure-core INTERFACE "-framework CoreFoundation")
    endif()
    if (WIN32)
        target_link_libraries(AZURE::azure-core INTERFACE winhttp.lib)
    endif()

    add_library(AZURE::azure-identity STATIC IMPORTED)
    set_target_properties(AZURE::azure-identity PROPERTIES IMPORTED_LOCATION "${AZURE_IDENTITY_LIB}")
    add_dependencies(AZURE::azure-identity azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-identity INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-identity INTERFACE LibXml2::LibXml2 CURL::libcurl OpenSSL::Crypto OpenSSL::SSL Threads::Threads nlohmann_json::nlohmann_json)
    if (APPLE)
        target_link_libraries(AZURE::azure-identity INTERFACE "-framework CoreFoundation")
    endif()
    if (WIN32)
        target_link_libraries(AZURE::azure-identity INTERFACE winhttp.lib)
    endif()

    add_library(AZURE::azure-storage-common STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-common PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_COMMON_LIB}")
    add_dependencies(AZURE::azure-storage-common azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-storage-common INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-storage-common INTERFACE LibXml2::LibXml2 CURL::libcurl OpenSSL::Crypto OpenSSL::SSL Threads::Threads nlohmann_json::nlohmann_json)
    if (APPLE)
        target_link_libraries(AZURE::azure-storage-common INTERFACE "-framework CoreFoundation")
    endif()
    if (WIN32)
        target_link_libraries(AZURE::azure-storage-common INTERFACE winhttp.lib)
    endif()

    add_library(AZURE::azure-storage-blobs STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-blobs PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_BLOBS_LIB}")
    add_dependencies(AZURE::azure-storage-blobs azure-sdk-cpp-external-build)
    target_include_directories(AZURE::azure-storage-blobs INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-storage-blobs INTERFACE LibXml2::LibXml2 CURL::libcurl OpenSSL::Crypto OpenSSL::SSL Threads::Threads nlohmann_json::nlohmann_json)
    if (APPLE)
        target_link_libraries(AZURE::azure-storage-blobs INTERFACE "-framework CoreFoundation")
    endif()
    if (WIN32)
        target_link_libraries(AZURE::azure-storage-blobs INTERFACE winhttp.lib)
    endif()
    add_definitions("-DBUILD_CURL_HTTP_TRANSPORT_ADAPTER")
endfunction(use_bundled_libazure)
