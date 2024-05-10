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

function(use_bundled_iodbc SOURCE_DIR BINARY_DIR)
    message("Using bundled iodbc")

    if(USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt iODBC external lib")
        include(${CMAKE_BINARY_DIR}/iodbc-config.cmake)

        set(IODBC_INCLUDE_DIRS "${iodbc_INCLUDE_DIRS}" CACHE STRING "" FORCE)
        set(IODBC_LIBRARIES "${iodbc_LIBRARIES}" CACHE STRING "" FORCE)

    elseif(USE_CMAKE_FETCH_CONTENT)
        message("Using CMAKE's ExternalProject_Add to manage source building iODBC external lib")

        if (WIN32)
            find_package(ODBC REQUIRED)
        else()

            # Define byproducts
            set(IODBC_BYPRODUCT "lib/libiodbc.a")

            set(IODBC_BYPRODUCT_DIR "${BINARY_DIR}/thirdparty/iodbc-install/")

            # Build project
            set(IODBC_URL https://github.com/openlink/iODBC/archive/v3.52.14.tar.gz)
            set(IODBC_URL_HASH "SHA256=896d7e16b283cf9a6f5b5f46e8e9549aef21a11935726b0170987cd4c59d16db")

            ExternalProject_Add(
                    iodbc-external
                    URL ${IODBC_URL}
                    URL_HASH ${IODBC_URL_HASH}
                    BUILD_IN_SOURCE true
                    SOURCE_DIR "${BINARY_DIR}/thirdparty/iodbc-src"
                    BUILD_COMMAND make
                    CMAKE_COMMAND ""
                    UPDATE_COMMAND ""
                    INSTALL_COMMAND make install
                    CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${IODBC_BYPRODUCT_DIR} --with-pic
                    STEP_TARGETS build
                    BUILD_BYPRODUCTS "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}"
                    EXCLUDE_FROM_ALL TRUE
            )

            # Set variables
            set(IODBC_FOUND "YES" CACHE STRING "" FORCE)
            set(IODBC_INCLUDE_DIRS "${IODBC_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
            set(IODBC_LIBRARIES "${IODBC_BYPRODUCT_DIR}/${IODBC_BYPRODUCT}" CACHE STRING "" FORCE)
        
            # Set exported variables for FindPackage.cmake
            set(EXPORTED_IODBC_INCLUDE_DIRS "${IODBC_INCLUDE_DIRS}" CACHE STRING "" FORCE)
            set(EXPORTED_IODBC_LIBRARIES "${IODBC_LIBRARIES}" CACHE STRING "" FORCE)

            # Create imported targets
            file(MAKE_DIRECTORY ${IODBC_INCLUDE_DIRS})

            add_library(ODBC::ODBC STATIC IMPORTED)
            set_target_properties(ODBC::ODBC PROPERTIES IMPORTED_LOCATION "${IODBC_LIBRARIES}")
            add_dependencies(ODBC::ODBC iodbc-external)
            set_property(TARGET ODBC::ODBC APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${IODBC_INCLUDE_DIRS})
        endif()
    endif()
endfunction(use_bundled_iodbc)
