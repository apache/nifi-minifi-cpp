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

include(FetchContent)

set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY "https://github.com/Mbed-TLS/mbedtls.git"
    GIT_TAG "v3.6.6"
    GIT_SUBMODULES "framework"
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(mbedtls)

foreach(mbedtls_target mbedtls mbedx509 mbedcrypto everest p256m)
    if(TARGET ${mbedtls_target})
        target_compile_options(${mbedtls_target} PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-error>)
    endif()
endforeach()

if (WIN32)
    set(MBEDTLS_BYPRODUCT_PREFIX "" CACHE STRING "" FORCE)
    set(MBEDTLS_BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
else()
    set(MBEDTLS_BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
    set(MBEDTLS_BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
endif()

# Set variables
set(MBEDTLS_FOUND "YES" CACHE STRING "" FORCE)
set(MBEDTLS_INCLUDE_DIRS "${mbedtls_SOURCE_DIR}/include" CACHE STRING "" FORCE)
set(MBEDTLS_LIBRARY "${mbedtls_BINARY_DIR}/library/${MBEDTLS_BYPRODUCT_PREFIX}mbedtls${MBEDTLS_BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
set(MBEDX509_LIBRARY "${mbedtls_BINARY_DIR}/library/${MBEDTLS_BYPRODUCT_PREFIX}mbedx509${MBEDTLS_BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
set(MBEDCRYPTO_LIBRARY "${mbedtls_BINARY_DIR}/library/${MBEDTLS_BYPRODUCT_PREFIX}mbedcrypto${MBEDTLS_BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
set(MBEDTLS_LIBRARIES "${MBEDTLS_LIBRARY};${MBEDX509_LIBRARY};${MBEDCRYPTO_LIBRARY}" CACHE STRING "" FORCE)

# Set exported variables for FindPackage.cmake
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_INCLUDE_DIRS=${MBEDTLS_INCLUDE_DIRS}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_LIBRARIES=${MBEDTLS_LIBRARIES_EXPORT}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDTLS_LIBRARY=${MBEDTLS_LIBRARY}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDX509_LIBRARY=${MBEDX509_LIBRARY}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_MBEDCRYPTO_LIBRARY=${MBEDCRYPTO_LIBRARY}" CACHE STRING "" FORCE)
