#
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
#

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(controllerservices)

file(GLOB SOURCES "*.cpp" "s3/*.cpp" "controllerservices/*.cpp")

set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/aws-sdk-cpp")
set(BYPRODUCT "${CMAKE_CURRENT_BINARY_DIR}/aws-sdk-install/")


if(STATIC_BUILD)
    set(MINIFI_AWS_BUILD_SHARED_LIBS "OFF")
    set(MINIFI_AWS_UNITY_BUILD "ON")
else()
    set(MINIFI_AWS_BUILD_SHARED_LIBS "ON")
    set(MINIFI_AWS_UNITY_BUILD "OFF")
endif()

# AWS SDK 1.7.89
ExternalProject_Add(
        awssdk-external
        GIT_REPOSITORY "https://github.com/aws/aws-sdk-cpp.git"
        GIT_TAG "66666d9437429c28927576cb5c898e490f8c8cf9"
        EXCLUDE_FROM_ALL TRUE
        INSTALL_DIR ${BYPRODUCT}
        CMAKE_ARGS -DBUILD_ONLY=s3
        -DENABLE_TESTING=OFF
        -DBUILD_SHARED_LIBS=${MINIFI_AWS_BUILD_SHARED_LIBS}
        -DENABLE_UNITY_BUILD=${MINIFI_AWS_UNITY_BUILD}
        -DCMAKE_BUILD_TYPE=RelWithDebInfo
        -DCMAKE_INSTALL_PREFIX=${BYPRODUCT}
)

add_library(awssdklib STATIC IMPORTED)
set_target_properties(awssdklib PROPERTIES IMPORTED_LOCATION "${BYPRODUCT}")

set(AWSSDK_FOUND "YES" CACHE STRING "" FORCE)
set(AWSSDK_INCLUDE_DIRS "${BYPRODUCT}/include" CACHE STRING "" FORCE)
set(AWSSDK_LIBRARIES awssdklib CACHE STRING "" FORCE)
set(AWSSDK_LIBRARY awssdklib CACHE STRING "" FORCE)

include_directories(${AWSSDK_INCLUDE_DIRS})
add_library(minifi-aws STATIC ${SOURCES})

if(STATIC_BUILD)
    set(SUFFIX "a")
else()
    if(APPLE)
        set(SUFFIX "dylib")
    else()
        set(SUFFIX "so")
    endif()
endif()


if (APPLE)
    if(STATIC_BUILD)
        target_link_libraries(minifi-aws "-framework CoreFoundation")
    endif()
endif()

target_link_libraries(minifi-aws ${BYPRODUCT}lib/libaws-c-common.${SUFFIX})
target_link_libraries(minifi-aws ${BYPRODUCT}lib/libaws-checksums.${SUFFIX})
target_link_libraries(minifi-aws ${BYPRODUCT}lib/libaws-c-event-stream.${SUFFIX})
target_link_libraries(minifi-aws ${BYPRODUCT}lib/libaws-cpp-sdk-core.${SUFFIX})
target_link_libraries(minifi-aws ${BYPRODUCT}lib/libaws-cpp-sdk-s3.${SUFFIX})

add_dependencies(minifi-aws awssdk-external)
SET (AWS-EXTENSION minifi-aws PARENT_SCOPE)
register_extension(minifi-aws)