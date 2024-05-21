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

function(use_bundled_soci SOURCE_DIR BINARY_DIR)
    message("Using bundled soci")

    if(USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt SOCI external lib")
        include(${CMAKE_BINARY_DIR}/SOCIConfig.cmake)

    elseif(USE_CMAKE_FETCH_CONTENT)
        message("Using CMAKE's ExternalProject_Add to manage source building SOCI external lib")

        # Build SOCI

        # Define byproducts
        # This should be based on GNUInstallDirs, but it's done wrong in Soci:
        # https://github.com/SOCI/soci/blob/release/4.0/CMakeLists.txt#L140
        if(APPLE OR CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(LIBDIR "lib")
        else()
            set(LIBDIR "lib64")
        endif()

        # Define byproducts

        if (WIN32)
            set(BYPRODUCT_SUFFIX "_4_0.lib")
        else()
            set(BYPRODUCT_SUFFIX ".a")
        endif()

        set(SOCI_BYPRODUCTS
            "${LIBDIR}/libsoci_core${BYPRODUCT_SUFFIX}"
            "${LIBDIR}/libsoci_odbc${BYPRODUCT_SUFFIX}"
            )

        set(SOCI_BYPRODUCT_DIR "${BINARY_DIR}/thirdparty/soci-install")

        foreach(SOCI_BYPRODUCT ${SOCI_BYPRODUCTS})
            list(APPEND SOCI_LIBRARIES_LIST "${SOCI_BYPRODUCT_DIR}/${SOCI_BYPRODUCT}")
        endforeach(SOCI_BYPRODUCT)


        # Build project
        if(WIN32)
            # Set build options
            set(SOCI_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${SOCI_BYPRODUCT_DIR}"
                "-DSOCI_TESTS=OFF"
                "-DSOCI_SHARED=OFF"
                "-DSOCI_CXX_C11=ON"
                "-DWITH_ODBC=ON"
                "-DWITH_BOOST=OFF")
        else()
            # SOCI has its own FindODBC.cmake file
            # Set build options
            set(SOCI_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
                "-DCMAKE_INSTALL_PREFIX=${SOCI_BYPRODUCT_DIR}"
                "-DSOCI_TESTS=OFF"
                "-DSOCI_SHARED=OFF"
                "-DSOCI_CXX_C11=ON"
                "-DSOCI_ODBC=ON"
                "-DODBC_INCLUDE_DIR=${IODBC_INCLUDE_DIRS}"
                "-DODBC_LIBRARY=${IODBC_LIBRARIES}"
                "-DWITH_BOOST=OFF")
        endif()

        if(NOT WIN32)
            list(APPEND SOCI_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${SOURCE_DIR}/cmake/"
                "-DEXPORTED_IODBC_INCLUDE_DIRS=${EXPORTED_IODBC_INCLUDE_DIRS}"
                "-DEXPORTED_IODBC_LIBRARIES=${EXPORTED_IODBC_LIBRARIES}")
        endif()

        set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/soci/all/patches/sqlite3-path.patch")



        set(SOCI_URL https://github.com/SOCI/soci/archive/4.0.1.tar.gz)
        set(SOCI_URL_HASH "SHA256=fa69347b1a1ef74450c0382b665a67bd6777cc7005bbe09726479625bcf1e29c")

        ExternalProject_Add(
            soci-external
            URL ${SOCI_URL}
            URL_HASH ${SOCI_URL_HASH}
            SOURCE_DIR "${BINARY_DIR}/thirdparty/soci-src"
            CMAKE_ARGS ${SOCI_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS ${SOCI_LIBRARIES_LIST}
            EXCLUDE_FROM_ALL TRUE
        )

        # Set dependencies
        if(NOT WIN32)
            add_dependencies(soci-external ODBC::ODBC)
        endif()


        # Set variables
        set(SOCI_FOUND "YES" CACHE STRING "" FORCE)
        set(SOCI_INCLUDE_DIR "${SOCI_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
        set(SOCI_LIBRARIES "${SOCI_LIBRARIES_LIST}" CACHE STRING "" FORCE)

        # Create imported targets
        file(MAKE_DIRECTORY ${SOCI_INCLUDE_DIR})

        add_library(SOCI::libsoci_core STATIC IMPORTED)
        set_target_properties(SOCI::libsoci_core PROPERTIES IMPORTED_LOCATION "${SOCI_BYPRODUCT_DIR}/${LIBDIR}/libsoci_core${BYPRODUCT_SUFFIX}")
        set_target_properties(SOCI::libsoci_core PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOCI_INCLUDE_DIR}")
        add_dependencies(SOCI::libsoci_core soci-external)
        target_compile_features(SOCI::libsoci_core INTERFACE cxx_std_14)
        
        add_library(SOCI::libsoci_odbc STATIC IMPORTED)
        set_target_properties(SOCI::libsoci_odbc PROPERTIES IMPORTED_LOCATION "${SOCI_BYPRODUCT_DIR}/${LIBDIR}/libsoci_odbc${BYPRODUCT_SUFFIX}")
        set_target_properties(SOCI::libsoci_odbc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOCI_INCLUDE_DIR}")
        add_dependencies(SOCI::libsoci_odbc soci-external)
        set_property(TARGET SOCI::libsoci_odbc APPEND PROPERTY INTERFACE_LINK_LIBRARIES SOCI::libsoci_core)
        set_property(TARGET SOCI::libsoci_odbc APPEND PROPERTY INTERFACE_LINK_LIBRARIES ODBC::ODBC)
        target_compile_features(SOCI::libsoci_odbc INTERFACE cxx_std_14)
    endif()
endfunction(use_bundled_soci)
