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
    set(PATCH_FILE1 "${SOURCE_DIR}/thirdparty/aws-sdk-cpp/c++20-compilation-fixes.patch")
    set(PATCH_FILE2 "${SOURCE_DIR}/thirdparty/aws-sdk-cpp/dll-export-injection.patch")
    set(PATCH_FILE3 "${SOURCE_DIR}/thirdparty/aws-sdk-cpp/shutdown-fix.patch")
    set(AWS_SDK_CPP_PATCH_COMMAND ${Bash_EXECUTABLE} -c "set -x &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE1}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE1}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE2}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE2}\") &&\
            (\"${Patch_EXECUTABLE}\" -p1 -R -s -f --dry-run -i \"${PATCH_FILE3}\" || \"${Patch_EXECUTABLE}\" -p1 -N -i \"${PATCH_FILE3}\") ")

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

    if (NOT WIN32 AND NOT APPLE)
        list(APPEND BYPRODUCTS "${CMAKE_INSTALL_LIBDIR}/${PREFIX}s2n.${SUFFIX}")
    endif()
    list(APPEND BYPRODUCTS
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-checksums.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-event-stream.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-s3.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-crt-cpp.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-common.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-mqtt.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-io.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-http.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-auth.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-cal.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-compression.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-core.${SUFFIX}"
            "${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-s3.${SUFFIX}")

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND AWSSDK_LIBRARIES_LIST "${BINARY_DIR}/thirdparty/libaws-install/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    set(AWS_SDK_CPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DBUILD_ONLY=s3
            -DENABLE_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_UNITY_BUILD=${AWS_ENABLE_UNITY_BUILD})

    if(WIN32)
        list(APPEND AWS_SDK_CPP_CMAKE_ARGS -DFORCE_EXPORT_CORE_API=ON -DFORCE_EXPORT_S3_API=ON)
    endif()

    append_third_party_passthrough_args(AWS_SDK_CPP_CMAKE_ARGS "${AWS_SDK_CPP_CMAKE_ARGS}")

    ExternalProject_Add(
            aws-sdk-cpp-external
            GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
            GIT_TAG "1.9.65"
            UPDATE_COMMAND git submodule update --init --recursive
            SOURCE_DIR "${BINARY_DIR}/thirdparty/aws-sdk-cpp-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${AWS_SDK_CPP_CMAKE_ARGS}
            PATCH_COMMAND ${AWS_SDK_CPP_PATCH_COMMAND}
            BUILD_BYPRODUCTS "${AWSSDK_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(aws-sdk-cpp-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)

    # Set variables
    set(LIBAWS_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAWS_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libaws-install/include" CACHE STRING "" FORCE)
    set(LIBAWS_LIBRARIES
            ${AWSSDK_LIBRARIES_LIST}
            CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-common STATIC IMPORTED)
    set_target_properties(AWS::aws-c-common PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-common.${SUFFIX}")
    add_dependencies(AWS::aws-c-common aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-common INTERFACE ${LIBAWS_INCLUDE_DIR})

    if (NOT WIN32 AND NOT APPLE)
        add_library(AWS::s2n STATIC IMPORTED)
        set_target_properties(AWS::s2n PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}s2n.${SUFFIX}")
        add_dependencies(AWS::s2n aws-sdk-cpp-external)
        target_include_directories(AWS::s2n INTERFACE ${LIBAWS_INCLUDE_DIR})
    endif()

    add_library(AWS::aws-c-io STATIC IMPORTED)
    set_target_properties(AWS::aws-c-io PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-io.${SUFFIX}")
    add_dependencies(AWS::aws-c-io aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-io INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-c-io INTERFACE AWS::aws-c-common)
    if (WIN32)
        target_link_libraries(AWS::aws-c-io INTERFACE ncrypt.lib)
    endif()

    add_library(AWS::aws-checksums STATIC IMPORTED)
    set_target_properties(AWS::aws-checksums PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-checksums.${SUFFIX}")
    add_dependencies(AWS::aws-checksums aws-sdk-cpp-external)
    target_include_directories(AWS::aws-checksums INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-event-stream STATIC IMPORTED)
    set_target_properties(AWS::aws-c-event-stream PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-event-stream.${SUFFIX}")
    add_dependencies(AWS::aws-c-event-stream aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-event-stream INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-c-event-stream INTERFACE AWS::aws-checksums AWS::aws-c-io)

    add_library(AWS::aws-c-auth STATIC IMPORTED)
    set_target_properties(AWS::aws-c-auth PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-auth.${SUFFIX}")
    add_dependencies(AWS::aws-c-auth aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-auth INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-s3 STATIC IMPORTED)
    set_target_properties(AWS::aws-c-s3 PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-s3.${SUFFIX}")
    add_dependencies(AWS::aws-c-s3 aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-s3 INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-c-s3 INTERFACE AWS::aws-c-auth)

    add_library(AWS::aws-c-mqtt STATIC IMPORTED)
    set_target_properties(AWS::aws-c-mqtt PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-mqtt.${SUFFIX}")
    add_dependencies(AWS::aws-c-mqtt aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-mqtt INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-http STATIC IMPORTED)
    set_target_properties(AWS::aws-c-http PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-http.${SUFFIX}")
    add_dependencies(AWS::aws-c-http aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-http INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-cal STATIC IMPORTED)
    set_target_properties(AWS::aws-c-cal PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-cal.${SUFFIX}")
    add_dependencies(AWS::aws-c-cal aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-cal INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-c-compression STATIC IMPORTED)
    set_target_properties(AWS::aws-c-compression PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-c-compression.${SUFFIX}")
    add_dependencies(AWS::aws-c-compression aws-sdk-cpp-external)
    target_include_directories(AWS::aws-c-compression INTERFACE ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::aws-crt-cpp STATIC IMPORTED)
    set_target_properties(AWS::aws-crt-cpp PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-crt-cpp.${SUFFIX}")
    add_dependencies(AWS::aws-crt-cpp aws-sdk-cpp-external)
    target_include_directories(AWS::aws-crt-cpp INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-crt-cpp INTERFACE AWS::aws-c-io AWS::aws-c-s3 AWS::aws-c-mqtt AWS::aws-c-http AWS::aws-c-cal AWS::aws-c-compression)

    add_library(AWS::aws-cpp-sdk-core STATIC IMPORTED)
    set_target_properties(AWS::aws-cpp-sdk-core PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-core.${SUFFIX}")
    add_dependencies(AWS::aws-cpp-sdk-core aws-sdk-cpp-external)
    target_include_directories(AWS::aws-cpp-sdk-core INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-cpp-sdk-core INTERFACE AWS::aws-crt-cpp AWS::aws-c-event-stream CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB Threads::Threads)
    if (APPLE)
        target_link_libraries(AWS::aws-cpp-sdk-core INTERFACE "-framework CoreFoundation -framework Security")
    elseif (WIN32)
        target_link_libraries(AWS::aws-cpp-sdk-core INTERFACE userenv.lib ws2_32.lib Wininet.lib winhttp.lib bcrypt.lib version.lib Secur32 Crypt32 Shlwapi)
    else()
        target_link_libraries(AWS::aws-cpp-sdk-core INTERFACE AWS::s2n)
    endif()

    add_library(AWS::aws-cpp-sdk-s3 STATIC IMPORTED)
    set_target_properties(AWS::aws-cpp-sdk-s3 PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/${CMAKE_INSTALL_LIBDIR}/${PREFIX}aws-cpp-sdk-s3.${SUFFIX}")
    add_dependencies(AWS::aws-cpp-sdk-s3 aws-sdk-cpp-external)
    target_include_directories(AWS::aws-cpp-sdk-s3 INTERFACE ${LIBAWS_INCLUDE_DIR})
    target_link_libraries(AWS::aws-cpp-sdk-s3 INTERFACE AWS::aws-cpp-sdk-core)
endfunction(use_bundled_libaws)
