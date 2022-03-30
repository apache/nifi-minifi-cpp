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
    set(INSTALL_DIR "${BINARY_DIR}/thirdparty/azure-sdk-cpp-install")
    if (WIN32)
        set(LIBDIR "lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
    endif()
    if (WIN32)
        set(SUFFIX "lib")
        set(PREFIX "")
        set(AZURE_CORE_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-identity.${SUFFIX}")
        set(AZURE_STORAGE_FILES_DATALAKE_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-files-datalake.${SUFFIX}")
    else()
        set(SUFFIX "a")
        set(PREFIX "lib")
        set(AZURE_CORE_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-core.${SUFFIX}")
        set(AZURE_STORAGE_COMMON_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-common.${SUFFIX}")
        set(AZURE_STORAGE_BLOBS_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-blobs.${SUFFIX}")
        set(AZURE_IDENTITY_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-identity.${SUFFIX}")
        set(AZURE_STORAGE_FILES_DATALAKE_LIB "${INSTALL_DIR}/${LIBDIR}/${PREFIX}azure-storage-files-datalake.${SUFFIX}")
    endif()

    set(AZURESDK_LIBRARIES_LIST
            "${AZURE_CORE_LIB}"
            "${AZURE_STORAGE_COMMON_LIB}"
            "${AZURE_STORAGE_BLOBS_LIB}"
            "${AZURE_IDENTITY_LIB}"
            "${AZURE_STORAGE_FILES_DATALAKE_LIB}")

    set(AZURE_SDK_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
        -DWARNINGS_AS_ERRORS=OFF
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR})
    append_third_party_passthrough_args(AZURE_SDK_CMAKE_ARGS "${AZURE_SDK_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            asdkext  # short for azure-sdk-cpp-external due to windows MAX_PATH limitations
            URL https://github.com/Azure/azure-sdk-for-cpp/archive/refs/tags/azure-storage-files-datalake_12.2.0.tar.gz
            URL_HASH "SHA256=d4e80ea5e786dc689ddd04825d97ab91f5e1ef2787fa88a3d5ee00f0b820433f"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/azure-sdk-cpp-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/azure-sdk-cpp-install"
            BUILD_BYPRODUCTS "${AZURESDK_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
            CMAKE_ARGS ${AZURE_SDK_CMAKE_ARGS}
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            PATCH_COMMAND ${PC}
    )

    # Set dependencies
    add_dependencies(asdkext CURL::libcurl LibXml2::LibXml2 OpenSSL::Crypto OpenSSL::SSL)

    # Set variables
    set(LIBAZURE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAZURE_INCLUDE_DIRS "${INSTALL_DIR}/include" CACHE STRING "" FORCE)
    set(LIBAZURE_LIBRARIES ${AZURESDK_LIBRARIES_LIST} CACHE STRING "" FORCE)

    # Create imported targets
    FOREACH(LIBAZURE_INCLUDE_DIR ${LIBAZURE_INCLUDE_DIRS})
        file(MAKE_DIRECTORY ${LIBAZURE_INCLUDE_DIR})
    ENDFOREACH(LIBAZURE_INCLUDE_DIR)

    add_library(AZURE::azure-core STATIC IMPORTED)
    set_target_properties(AZURE::azure-core PROPERTIES IMPORTED_LOCATION "${AZURE_CORE_LIB}")
    add_dependencies(AZURE::azure-core asdkext)
    target_include_directories(AZURE::azure-core INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-core INTERFACE CURL::libcurl OpenSSL::Crypto OpenSSL::SSL)
    if (WIN32)
        target_link_libraries(AZURE::azure-core INTERFACE winhttp.lib WebServices.lib)
    endif()

    add_library(AZURE::azure-identity STATIC IMPORTED)
    set_target_properties(AZURE::azure-identity PROPERTIES IMPORTED_LOCATION "${AZURE_IDENTITY_LIB}")
    add_dependencies(AZURE::azure-identity asdkext)
    target_include_directories(AZURE::azure-identity INTERFACE ${LIBAZURE_INCLUDE_DIRS})

    add_library(AZURE::azure-storage-common STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-common PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_COMMON_LIB}")
    add_dependencies(AZURE::azure-storage-common asdkext)
    target_include_directories(AZURE::azure-storage-common INTERFACE ${LIBAZURE_INCLUDE_DIRS})
    target_link_libraries(AZURE::azure-storage-common INTERFACE LibXml2::LibXml2)

    add_library(AZURE::azure-storage-blobs STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-blobs PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_BLOBS_LIB}")
    add_dependencies(AZURE::azure-storage-blobs asdkext)
    target_include_directories(AZURE::azure-storage-blobs INTERFACE ${LIBAZURE_INCLUDE_DIRS})

    add_library(AZURE::azure-storage-files-datalake STATIC IMPORTED)
    set_target_properties(AZURE::azure-storage-files-datalake PROPERTIES IMPORTED_LOCATION "${AZURE_STORAGE_FILES_DATALAKE_LIB}")
    add_dependencies(AZURE::azure-storage-files-datalake asdkext)
    target_include_directories(AZURE::azure-storage-files-datalake INTERFACE ${LIBAZURE_INCLUDE_DIRS})
endfunction(use_bundled_libazure)
