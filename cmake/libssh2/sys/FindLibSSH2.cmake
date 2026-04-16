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

if (TARGET libssh2)
    set(LIBSSH2_FOUND TRUE)
    set(LibSSH2_FOUND TRUE)
    return()
endif ()

if (NOT LIBSSH2_ROOT_DIR)
    message(FATAL_ERROR "Strict bundled libssh2 requires LIBSSH2_ROOT_DIR to be passed to this CMake scope!")
endif ()

find_library(LIBSSH2_LIBRARY
        NAMES ssh2 libssh2
        PATHS "${LIBSSH2_ROOT_DIR}/lib" "${LIBSSH2_ROOT_DIR}/lib64"
        NO_DEFAULT_PATH # Strictly prevent system fallback
)

set(LIBSSH2_INCLUDE_DIR "${LIBSSH2_ROOT_DIR}/include")

if (NOT LIBSSH2_LIBRARY OR NOT EXISTS "${LIBSSH2_INCLUDE_DIR}")
    message(FATAL_ERROR "Failed to locate bundled libssh2 components inside ${LIBSSH2_ROOT_DIR}")
endif ()

set(LIBSSH2_FOUND TRUE)
set(LibSSH2_FOUND TRUE)
set(LIBSSH2_INCLUDE_DIRS "${LIBSSH2_INCLUDE_DIR}")
set(LIBSSH2_LIBRARIES "${LIBSSH2_LIBRARY}")

add_library(libssh2 STATIC IMPORTED)
set_target_properties(libssh2 PROPERTIES
        IMPORTED_LOCATION "${LIBSSH2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBSSH2_INCLUDE_DIR}"
)
