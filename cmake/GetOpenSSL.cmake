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

if(MINIFI_OPENSSL_SOURCE STREQUAL "CONAN")
    message("Using Conan to install OpenSSL")
    find_package(OpenSSL REQUIRED)
    set(FIND_OPENSSL_PATH "${CMAKE_BINARY_DIR}/FindOpenSSL.cmake" CACHE INTERNAL "Location of the FindOpenSSL file, for other dependencies")
    set(FIND_CRYPTO_PATH "${CMAKE_BINARY_DIR}/FindOpenSSL.cmake" CACHE INTERNAL "Conan's FindOpenSSL finds the Crypto library, too")

    set(OPENSSL_BIN_DIR "${openssl_PACKAGE_FOLDER_RELEASE}" CACHE STRING "" FORCE)

    find_package(openssl-fips REQUIRED)
    set(OPENSSL_FIPS_BIN_DIR "${openssl-fips_PACKAGE_FOLDER_RELEASE}" CACHE STRING "" FORCE)

    if(APPLE OR WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 4 OR CMAKE_SYSTEM_PROCESSOR MATCHES "(arm64)|(ARM64)|(aarch64)|(armv8)")
        set(LIBDIR "lib")
    else()
        set(LIBDIR "lib64")
    endif()

    if (WIN32)
        set(BYPRODUCT_DYN_SUFFIX ".dll" CACHE STRING "" FORCE)
    elseif(APPLE)
        set(BYPRODUCT_DYN_SUFFIX ".dylib" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_DYN_SUFFIX ".so" CACHE STRING "" FORCE)
    endif()

    if (WIN32)
        set(EXECUTABLE_SUFFIX ".exe" CACHE STRING "" FORCE)
    else()
        set(EXECUTABLE_SUFFIX "" CACHE STRING "" FORCE)
    endif()

    set(FIPS_BYPRODUCTS "${LIBDIR}/ossl-modules/fips${BYPRODUCT_DYN_SUFFIX}")

    FOREACH(BYPRODUCT ${FIPS_BYPRODUCTS})
        LIST(APPEND OPENSSL_FIPS_FILE_LIST "${OPENSSL_FIPS_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
        install(FILES ${OPENSSL_FIPS_FILE_LIST}
                DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/fips
                COMPONENT bin)

        install(FILES "${OPENSSL_BIN_DIR}/bin/openssl${EXECUTABLE_SUFFIX}"
                DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/fips
                COMPONENT bin
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)

    elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
        install(FILES ${OPENSSL_FIPS_FILE_LIST}
                DESTINATION fips
                COMPONENT bin)

        install(FILES "${OPENSSL_BIN_DIR}/bin/openssl${EXECUTABLE_SUFFIX}"
                DESTINATION fips
                COMPONENT bin
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
    endif()

elseif(MINIFI_OPENSSL_SOURCE STREQUAL "BUILD")
    message("Using CMake to build OpenSSL from source")
    include(BundledOpenSSL)
    use_openssl(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/ssl")
endif()
