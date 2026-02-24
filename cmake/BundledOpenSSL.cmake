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

function(use_openssl SOURCE_DIR BINARY_DIR)
    message("Using bundled OpenSSL")

    if(APPLE OR WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 4 OR CMAKE_SYSTEM_PROCESSOR MATCHES "(arm64)|(ARM64)|(aarch64)|(armv8)")
        set(LIBDIR "lib")
    else()
        set(LIBDIR "lib64")
    endif()

    # Define byproducts
    set(BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
    set(OPENSSL_BUILD_SHARED "NO" CACHE STRING "" FORCE)

    if (WIN32)
        set(BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
    # Due to OpenSSL 3's static linking issue on x86 MacOS platform we make an exception to build a shared library instead
    elseif (APPLE AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64"))
        set(BYPRODUCT_SUFFIX ".dylib" CACHE STRING "" FORCE)
        set(OPENSSL_BUILD_SHARED "YES" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
    endif()

    if (WIN32)
        set(EXECUTABLE_SUFFIX ".exe" CACHE STRING "" FORCE)
    else()
        set(EXECUTABLE_SUFFIX "" CACHE STRING "" FORCE)
    endif()

    set(BYPRODUCTS
            "${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}"
            "${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}"
            )

    if (OPENSSL_BUILD_SHARED)
        set(OPENSSL_SHARED_FLAG "" CACHE STRING "" FORCE)
    else()
        set(OPENSSL_SHARED_FLAG "no-shared" CACHE STRING "" FORCE)
    endif()

    set(OPENSSL_EXTRA_FLAGS
            no-tests            # Disable tests
            no-capieng          # disable CAPI engine (legacy)
            no-docs             # disable docs and manpages
            no-legacy           # disable legacy modules
            enable-tfo          # Enable TCP Fast Open
            no-ssl              # disable SSLv3
            no-engine)          # disable Engine API as it is deprecated since OpenSSL 3.0 and not FIPS compatible

    set(OPENSSL_BIN_DIR "${BINARY_DIR}/thirdparty/openssl-install" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND OPENSSL_LIBRARIES_LIST "${OPENSSL_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    if (OPENSSL_BUILD_SHARED)
        install(FILES ${OPENSSL_LIBRARIES_LIST} DESTINATION bin COMPONENT bin)
    endif()

    # Set build options
    set(OPENSSL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${OPENSSL_BIN_DIR}"
            "-DCMAKE_POLICY_DEFAULT_CMP0063=NEW"
            # avoid polluting the global namespace, otherwise could interfere with the system openssl (e.g. python script extension using numpy)
            "-DCMAKE_C_VISIBILITY_PRESET=hidden"
            "-DCMAKE_CXX_VISIBILITY_PRESET=hidden"
            "-DCMAKE_VISIBILITY_INLINES_HIDDEN=ON"
            )

    set(OPENSSL_VERSION "3.3.6" CACHE STRING "" FORCE)

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
                openssl-external
                URL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
                URL_HASH "SHA256=22db04f3c8f9a808c9795dcf7d2713ff40c12c410ea2d1f6435c6c9c8558958b"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND perl Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" ${OPENSSL_SHARED_FLAG} ${OPENSSL_EXTRA_FLAGS} "--prefix=${OPENSSL_BIN_DIR}" "--openssldir=${OPENSSL_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_LIBRARIES_LIST}
                EXCLUDE_FROM_ALL TRUE
                BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
                INSTALL_COMMAND nmake install
                DOWNLOAD_NO_PROGRESS TRUE
                TLS_VERIFY TRUE
            )
    else()
        ExternalProject_Add(
                openssl-external
                URL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
                URL_HASH "SHA256=22db04f3c8f9a808c9795dcf7d2713ff40c12c410ea2d1f6435c6c9c8558958b"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND ./Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" ${OPENSSL_SHARED_FLAG} ${OPENSSL_EXTRA_FLAGS} "--prefix=${OPENSSL_BIN_DIR}" "--openssldir=${OPENSSL_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_LIBRARIES_LIST}
                EXCLUDE_FROM_ALL TRUE
                DOWNLOAD_NO_PROGRESS TRUE
                TLS_VERIFY TRUE
        )
    endif()

    # Set variables
    set(OPENSSL_FOUND "YES" CACHE STRING "" FORCE)
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES_LIST};${CMAKE_DL_LIBS}"  CACHE STRING "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
    set(OPENSSL_SSL_LIBRARY "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    string(REPLACE ";" "%" OPENSSL_LIBRARIES_EXPORT "${OPENSSL_LIBRARIES}")
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_LIBRARIES=${OPENSSL_LIBRARIES_EXPORT}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENSSL_VERSION=${OPENSSL_VERSION}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

    add_library(OpenSSL::Crypto STATIC IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::Crypto openssl-external)

    add_library(OpenSSL::SSL STATIC IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${OPENSSL_BIN_DIR}/${LIBDIR}/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}")
    add_dependencies(OpenSSL::SSL openssl-external)
    set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)

    if(WIN32)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib )
        set_property(TARGET OpenSSL::SSL APPEND PROPERTY INTERFACE_LINK_LIBRARIES crypt32.lib)
    endif()

    if (WIN32)
        set(BYPRODUCT_DYN_SUFFIX ".dll" CACHE STRING "" FORCE)
    elseif(APPLE)
        set(BYPRODUCT_DYN_SUFFIX ".dylib" CACHE STRING "" FORCE)
    else()
        set(BYPRODUCT_DYN_SUFFIX ".so" CACHE STRING "" FORCE)
    endif()

    set(FIPS_BYPRODUCTS
            "${LIBDIR}/ossl-modules/fips${BYPRODUCT_DYN_SUFFIX}"
            )

    set(OPENSSL_FIPS_BIN_DIR "${BINARY_DIR}/thirdparty/openssl-fips-install" CACHE STRING "" FORCE)

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
                URL https://github.com/openssl/openssl/releases/download/openssl-3.0.9/openssl-3.0.9.tar.gz
                URL_HASH "SHA256=eb1ab04781474360f77c318ab89d8c5a03abc38e63d65a603cabbf1b00a1dc90"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-fips-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND perl Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} ${OPENSSL_WINDOWS_COMPILE_FLAGS}" ${OPENSSL_SHARED_FLAG} ${OPENSSL_FIPS_EXTRA_FLAGS} enable-fips "--prefix=${OPENSSL_FIPS_BIN_DIR}" "--openssldir=${OPENSSL_FIPS_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_FIPS_FILE_LIST}
                EXCLUDE_FROM_ALL TRUE
                BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
                INSTALL_COMMAND nmake install_fips
            )
    else()
        ExternalProject_Add(
            openssl-fips-external
                URL https://github.com/openssl/openssl/releases/download/openssl-3.0.9/openssl-3.0.9.tar.gz
                URL_HASH "SHA256=eb1ab04781474360f77c318ab89d8c5a03abc38e63d65a603cabbf1b00a1dc90"
                SOURCE_DIR "${BINARY_DIR}/thirdparty/openssl-fips-src"
                BUILD_IN_SOURCE true
                CONFIGURE_COMMAND ./Configure "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}" "CFLAGS=${PASSTHROUGH_CMAKE_C_FLAGS} -fPIC" "CXXFLAGS=${PASSTHROUGH_CMAKE_CXX_FLAGS} -fPIC" ${OPENSSL_SHARED_FLAG} ${OPENSSL_FIPS_EXTRA_FLAGS}  "--prefix=${OPENSSL_FIPS_BIN_DIR}" "--openssldir=${OPENSSL_FIPS_BIN_DIR}"
                BUILD_BYPRODUCTS ${OPENSSL_FIPS_FILE_LIST}
                EXCLUDE_FROM_ALL TRUE
                INSTALL_COMMAND make install_fips
        )
    endif()

    add_dependencies(OpenSSL::Crypto openssl-fips-external)

endfunction(use_openssl)
