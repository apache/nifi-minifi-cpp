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

function(use_bundled_libaws SOURCE_DIR BINARY_DIR)
    set(PATCH_FILE "${SOURCE_DIR}/thirdparty/aws-sdk-cpp/core-wstr-fix.patch")
    set(AWS_SDK_CPP_PATCH_COMMAND "${Patch_EXECUTABLE}" -p1 -R -s -f --dry-run -i "${PATCH_FILE}" || "${Patch_EXECUTABLE}" -p1 -N -i "${PATCH_FILE}")

    if (WIN32)
        set(CMAKE_INSTALL_LIBDIR "lib")
    else()
        include(GNUInstallDirs)
    endif()

    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
        set(PREFIX "")
    else()
        set(SUFFIX "a")
        set(PREFIX "lib")
    endif()
    set(BYPRODUCTS
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-core.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-s3.${SUFFIX}")

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND AWSSDK_LIBRARIES_LIST "${BINARY_DIR}/thirdparty/libaws-install/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(AWS_C_COMMON_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DENABLE_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF)

    append_third_party_passthrough_args(AWS_C_COMMON_CMAKE_ARGS "${AWS_C_COMMON_CMAKE_ARGS}")

    set(AWS_CHECKSUM_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DBUILD_SHARED_LIBS=OFF)

    append_third_party_passthrough_args(AWS_CHECKSUM_CMAKE_ARGS "${AWS_CHECKSUM_CMAKE_ARGS}")

    set(AWS_C_EVENT_STREAM_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_MODULE_PATH=${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/cmake/
            -DBUILD_SHARED_LIBS=OFF)

    append_third_party_passthrough_args(AWS_C_EVENT_STREAM_CMAKE_ARGS "${AWS_C_EVENT_STREAM_CMAKE_ARGS}")

    set(AWS_SDK_CPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DBUILD_ONLY=s3
            -DENABLE_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_UNITY_BUILD=${AWS_ENABLE_UNITY_BUILD}
            -DBUILD_DEPS=OFF)

    append_third_party_passthrough_args(AWS_SDK_CPP_CMAKE_ARGS "${AWS_SDK_CPP_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            aws-c-common-external
            GIT_REPOSITORY "https://github.com/awslabs/aws-c-common.git"
            GIT_TAG "d8f6f067975cd3670c62cca0455b9d381db19756"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/aws-c-common-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${AWS_C_COMMON_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-common.${SUFFIX}"
            EXCLUDE_FROM_ALL TRUE
    )
    ExternalProject_Add(
            aws-checksum-external
            GIT_REPOSITORY "https://github.com/awslabs/aws-checksums.git"
            GIT_TAG "8e1a84c2924774db1b9d945c556343b217d71d05"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/aws-checksums-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${AWS_CHECKSUM_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-checksums.${SUFFIX}"
            EXCLUDE_FROM_ALL TRUE
    )
    ExternalProject_Add(
            aws-c-event-stream-external
            GIT_REPOSITORY "https://github.com/awslabs/aws-c-event-stream.git"
            GIT_TAG "3462b68d563d8f9b3a26517b833671a24ab81cc5"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/aws-c-event-stream-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${AWS_C_EVENT_STREAM_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-event-stream.${SUFFIX}"
            EXCLUDE_FROM_ALL TRUE
    )
    ExternalProject_Add(
            aws-sdk-cpp-external
            GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
            GIT_TAG "1.8.52"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/aws-sdk-cpp-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${AWS_SDK_CPP_CMAKE_ARGS}
            PATCH_COMMAND ${AWS_SDK_CPP_PATCH_COMMAND}
            BUILD_BYPRODUCTS "${AWSSDK_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(aws-c-common-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    add_dependencies(aws-checksum-external aws-c-common-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    add_dependencies(aws-c-event-stream-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    add_dependencies(aws-c-event-stream-external aws-c-common-external aws-checksum-external)
    add_dependencies(aws-sdk-cpp-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    add_dependencies(aws-sdk-cpp-external aws-c-event-stream-external aws-c-common-external aws-checksum-external)

    # Set variables
    set(LIBAWS_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAWS_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libaws-install/include" CACHE STRING "" FORCE)
    set(LIBAWS_LIBRARIES
            ${AWSSDK_LIBRARIES_LIST}
            "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-event-stream.${SUFFIX}"
            "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-common.${SUFFIX}"
            "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-checksums.${SUFFIX}"
            CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-common STATIC IMPORTED)
    set_target_properties(AWS::aws-c-common PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-common.${SUFFIX}")
    add_dependencies(AWS::aws-c-common aws-c-common-external)
    set_property(TARGET AWS::aws-c-common APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::aws-c-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        set_property(TARGET AWS::aws-c-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::aws-checksums STATIC IMPORTED)
    set_target_properties(AWS::aws-checksums PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-checksums.${SUFFIX}")
    add_dependencies(AWS::aws-checksums aws-checksums-external)
    set_property(TARGET AWS::aws-checksums APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::aws-checksums APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        set_property(TARGET AWS::aws-checksums APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::aws-c-event-stream STATIC IMPORTED)
    set_target_properties(AWS::aws-c-event-stream PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-event-stream.${SUFFIX}")
    add_dependencies(AWS::aws-c-event-stream aws-c-event-stream-external)
    set_property(TARGET AWS::aws-c-event-stream APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::aws-c-event-stream APPEND PROPERTY INTERFACE_LINK_LIBRARIES AWS::aws-c-common AWS::aws-checksums CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        set_property(TARGET AWS::aws-c-event-stream APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::aws-cpp-sdk-core STATIC IMPORTED)
    set_target_properties(AWS::aws-cpp-sdk-core PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-core.${SUFFIX}")
    add_dependencies(AWS::aws-cpp-sdk-core aws-sdk-cpp-external)
    set_property(TARGET AWS::aws-cpp-sdk-core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::aws-cpp-sdk-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES AWS::aws-c-event-stream AWS::aws-c-common AWS::aws-checksums CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        set_property(TARGET AWS::aws-cpp-sdk-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()
    if (WIN32)
        set_property(TARGET AWS::aws-cpp-sdk-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES userenv.lib ws2_32.lib Wininet.lib winhttp.lib bcrypt.lib version.lib)
    endif()

    add_library(AWS::aws-cpp-sdk-s3 STATIC IMPORTED)
    set_target_properties(AWS::aws-cpp-sdk-s3 PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-s3.${SUFFIX}")
    add_dependencies(AWS::aws-cpp-sdk-s3 aws-sdk-cpp-external)
    set_property(TARGET AWS::aws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::aws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads AWS::aws-cpp-sdk-core)
    if (APPLE)
        set_property(TARGET AWS::aws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()
endfunction(use_bundled_libaws)
