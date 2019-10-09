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

function(use_bundled_sqlite SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "libsqlite.lib")
    else()
        set(BYPRODUCT "libsqlite.a")
    endif()

    # Build project
    ExternalProject_Add(
            sqlite-external
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/sqlite"
            BINARY_DIR "${BINARY_DIR}/thirdparty/sqlite"
            CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/sqlite/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(SQLite3_FOUND "YES" CACHE STRING "" FORCE)
    set(SQLite3_INCLUDE_DIRS "${SOURCE_DIR}/thirdparty/sqlite" CACHE STRING "" FORCE)
    set(SQLite3_LIBRARY "${BINARY_DIR}/thirdparty/sqlite/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(SQLite3_LIBRARIES ${SQLite3_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(SQLite::SQLite3 STATIC IMPORTED)
    set_target_properties(SQLite::SQLite3 PROPERTIES IMPORTED_LOCATION "${SQLite3_LIBRARY}")
    add_dependencies(SQLite::SQLite3 sqlite-external)
    set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SQLite3_INCLUDE_DIRS})
endfunction(use_bundled_sqlite)
