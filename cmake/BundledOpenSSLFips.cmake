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

set(OPENSSL_FIPS_BIN_DIR "${CMAKE_BINARY_DIR}/thirdparty/openssl-fips-install" CACHE STRING "" FORCE)

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

set(OPENSSL_FIPS_EXTRA_FLAGS
        no-tests            # Disable tests
        no-capieng          # disable CAPI engine (legacy)
        no-legacy           # disable legacy modules
        no-ssl              # disable SSLv3
        no-engine           # disable Engine API as it is deprecated since OpenSSL 3.0 and not FIPS compatible
        enable-fips)        # enable FIPS module

if (WIN32)
    find_program(JOM_EXECUTABLE_PATH
        NAMES jom.exe
        PATHS ENV PATH
        NO_DEFAULT_PATH)
    if(JOM_EXECUTABLE_PATH)
        include(ProcessorCount)
        processorcount(jobs)
        set(OPENSSL_BUILD_COMMAND ${JOM_EXECUTABLE_PATH} -j${jobs})
        set(OPENSSL_WINDOWS_COMPILE_FLAGS /FS)
    else()
        message("Using nmake for OpenSSL build")
        set(OPENSSL_BUILD_COMMAND nmake)
        set(OPENSSL_WINDOWS_COMPILE_FLAGS "")
    endif()
    ExternalProject_Add(
            openssl-fips-external
            URL https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-3.1.2.tar.gz
            URL_HASH "SHA256=a0ce69b8b97ea6a35b96875235aa453b966ba3cba8af2de23657d8b6767d6539"
            SOURCE_DIR "${CMAKE_BINARY_DIR}/thirdparty/openssl-fips-src"
            BUILD_IN_SOURCE true
            CONFIGURE_COMMAND perl Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${OPENSSL_C_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" ${OPENSSL_SHARED_FLAG} ${OPENSSL_FIPS_EXTRA_FLAGS} enable-fips "--prefix=${OPENSSL_FIPS_BIN_DIR}" "--openssldir=${OPENSSL_FIPS_BIN_DIR}"
            BUILD_BYPRODUCTS ${OPENSSL_FIPS_FILE_LIST}
            EXCLUDE_FROM_ALL TRUE
            BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
            INSTALL_COMMAND nmake install_fips
        )
else()
    ExternalProject_Add(
        openssl-fips-external
            URL https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-3.1.2.tar.gz
            URL_HASH "SHA256=a0ce69b8b97ea6a35b96875235aa453b966ba3cba8af2de23657d8b6767d6539"
            SOURCE_DIR "${CMAKE_BINARY_DIR}/thirdparty/openssl-fips-src"
            BUILD_IN_SOURCE true
            CONFIGURE_COMMAND ./Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${OPENSSL_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" ${OPENSSL_SHARED_FLAG} ${OPENSSL_FIPS_EXTRA_FLAGS}  "--prefix=${OPENSSL_FIPS_BIN_DIR}" "--openssldir=${OPENSSL_FIPS_BIN_DIR}"
            BUILD_BYPRODUCTS ${OPENSSL_FIPS_FILE_LIST}
            EXCLUDE_FROM_ALL TRUE
            INSTALL_COMMAND make install_fips
    )
endif()

add_dependencies(OpenSSL::Crypto openssl-fips-external)
set(OPENSSL_ROOT_DIR "${OPENSSL_BIN_DIR}" CACHE INTERNAL "Strict single source of truth for bundled OpenSSL")
