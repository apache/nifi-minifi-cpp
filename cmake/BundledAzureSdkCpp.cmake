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
    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
        set(PREFIX "")
    else()
        set(SUFFIX "a")
        set(PREFIX "lib")
    endif()
    set(AZURESDK_LIBRARIES_LIST
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${PREFIX}azure-core.${SUFFIX}"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${PREFIX}azure-storage-common.${SUFFIX}"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${PREFIX}azure-storage-blobs.${SUFFIX}"
            "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${PREFIX}azure-identity.${SUFFIX}")

    # Build project
    ExternalProject_Add(
            azure-sdk-cpp-external
            GIT_REPOSITORY "https://github.com/Azure/azure-sdk-for-cpp.git"
            GIT_TAG "azure-storage-blobs_1.0.0-beta.4"
            BUILD_IN_SOURCE true
            SOURCE_DIR "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src"
            BUILD_BYPRODUCTS "${AZURESDK_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
            STEP_TARGETS build
    )

    # Set dependencies
    include(FindLibXml2)
    add_dependencies(azure-sdk-cpp-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL LibXml2::LibXml2)

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
    ENDFOREACH(BYPRODUCT)

    add_library(AZURE::azure-core STATIC IMPORTED)
    set_target_properties(AZURE::azure-core PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/core/azure-core/${PREFIX}azure-core.${SUFFIX}")
    add_dependencies(AZURE::azure-core azure-sdk-cpp-external-build)
    set_property(TARGET AZURE::azure-core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAZURE_INCLUDE_DIRS})
    set_property(TARGET AZURE::azure-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL LibXml2::LibXml2 Threads::Threads)
    if (APPLE)
        set_property(TARGET AZURE::azure-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AZURE::azure-identity STATIC IMPORTED)
    set_target_properties(AZURE::azure-identity PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/identity/azure-identity/${PREFIX}azure-identity.${SUFFIX}")
    add_dependencies(AZURE::azure-identity azure-sdk-cpp-external-build)
    set_property(TARGET AZURE::azure-identity APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAZURE_INCLUDE_DIRS})
    set_property(TARGET AZURE::azure-identity APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL LibXml2::LibXml2 Threads::Threads)
    if (APPLE)
        set_property(TARGET AZURE::azure-identity APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AZURE::azure-storage-common STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-common PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-common/${PREFIX}azure-storage-common.${SUFFIX}")
    add_dependencies(AZURE::azure-storage-common azure-sdk-cpp-external-build)
    set_property(TARGET AZURE::azure-storage-common APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAZURE_INCLUDE_DIRS})
    set_property(TARGET AZURE::azure-storage-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL LibXml2::LibXml2 Threads::Threads)
    if (APPLE)
        set_property(TARGET AZURE::azure-storage-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AZURE::azure-storage-blobs STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-blobs PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src/sdk/storage/azure-storage-blobs/${PREFIX}azure-storage-blobs.${SUFFIX}")
    add_dependencies(AZURE::azure-storage-blobs azure-sdk-cpp-external-build)
    set_property(TARGET AZURE::azure-storage-blobs APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAZURE_INCLUDE_DIRS})
    set_property(TARGET AZURE::azure-storage-blobs APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL LibXml2::LibXml2 Threads::Threads)
    if (APPLE)
        set_property(TARGET AZURE::azure-storage-blobs APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()
endfunction(use_bundled_libazure)
