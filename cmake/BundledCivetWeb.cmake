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

function(use_bundled_civetweb SOURCE_DIR BINARY_DIR)
    message("Using bundled civetweb")

    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/civetweb/civetweb.patch")

    set(LIBDIR "lib")
    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
    else()
        set(PREFIX "lib")
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
        set(SUFFIX "a")
    endif()

    set(BYPRODUCTS
            "${LIBDIR}/${PREFIX}civetweb.${SUFFIX}"
            "${LIBDIR}/${PREFIX}civetweb-cpp.${SUFFIX}"
            )

    set(CIVETWEB_BIN_DIR "${BINARY_DIR}/thirdparty/civetweb-install/" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND CIVETWEB_LIBRARIES_LIST "${CIVETWEB_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(CIVETWEB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CIVETWEB_BIN_DIR}"
            -DCIVETWEB_ENABLE_SSL_DYNAMIC_LOADING=OFF
            -DCIVETWEB_BUILD_TESTING=OFF
            -DCIVETWEB_ENABLE_DUKTAPE=OFF
            -DCIVETWEB_ENABLE_LUA=OFF
            -DCIVETWEB_ENABLE_CXX=ON
            -DCIVETWEB_ALLOW_WARNINGS=ON
            -DCIVETWEB_ENABLE_ASAN=OFF)
    if (OPENSSL_OFF)
        list(APPEND CIVETWEB_CMAKE_ARGS -DCIVETWEB_ENABLE_SSL=OFF)
    endif()

    append_third_party_passthrough_args(CIVETWEB_CMAKE_ARGS "${CIVETWEB_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            civetweb-external
            URL "https://github.com/civetweb/civetweb/archive/v1.12.tar.gz"
            URL_HASH "SHA256=8cab1e2ad8fb3e2e81fed0b2321a5afbd7269a644c44ed4c3607e0a212c6d9e1"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/civetweb-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CIVETWEB_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${CIVETWEB_LIBRARIES_LIST}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    if (NOT OPENSSL_OFF)
        add_dependencies(civetweb-external OpenSSL::SSL OpenSSL::Crypto)
    endif()

    # Set variables
    set(CIVETWEB_FOUND "YES" CACHE STRING "" FORCE)
    set(CIVETWEB_INCLUDE_DIR "${CIVETWEB_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(CIVETWEB_LIBRARIES "${CIVETWEB_BIN_DIR}/${LIBDIR}/${PREFIX}civetweb.${SUFFIX}" "${CIVETWEB_BIN_DIR}/${LIBDIR}/${PREFIX}civetweb-cpp.${SUFFIX}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_CIVETWEB_INCLUDE_DIR=${CIVETWEB_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_CIVETWEB_LIBRARIES=${CIVETWEB_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${CIVETWEB_INCLUDE_DIR})

    add_library(CIVETWEB::c-library STATIC IMPORTED GLOBAL)
    set_target_properties(CIVETWEB::c-library PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/${LIBDIR}/${PREFIX}civetweb.${SUFFIX}")
    set_property(TARGET CIVETWEB::c-library APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CIVETWEB_INCLUDE_DIR})
    add_dependencies(CIVETWEB::c-library civetweb-external)
    if (NOT OPENSSL_OFF)
        target_link_libraries(CIVETWEB::c-library INTERFACE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
    endif()

    add_library(CIVETWEB::civetweb-cpp STATIC IMPORTED GLOBAL)
    set_target_properties(CIVETWEB::civetweb-cpp PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/${LIBDIR}/${PREFIX}civetweb-cpp.${SUFFIX}")
    target_link_libraries(CIVETWEB::civetweb-cpp INTERFACE CIVETWEB::c-library)
    add_dependencies(CIVETWEB::civetweb-cpp civetweb-external)
endfunction(use_bundled_civetweb)
