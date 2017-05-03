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

# Find module for LIBGPS library and includes
#  LIBGPS_FOUND - if system found LIBGPS library
#  LIBGPS_INCLUDE_DIRS - The LIBGPS include directories
#  LIBGPS_LIBRARIES - The libraries needed to use LIBGPS
#  LIBGPS_DEFINITIONS - Compiler switches required for using LIBGPS

# For OS X do not attempt to use the OS X application frameworks or bundles.
set (CMAKE_FIND_FRAMEWORK NEVER)
set (CMAKE_FIND_APPBUNDLE NEVER)

find_path(LIBGPS_INCLUDE_DIR
    NAMES gps.h
    PATHS /usr/local/include /usr/include
    DOC "LIBGPS include header"
)

find_library(LIBGPS_LIBRARY 
    NAMES libgps.so libgps.dylib
    PATHS /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/arm-linux-gnueabihf
    DOC "LIBGPS library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBGPS DEFAULT_MSG LIBGPS_INCLUDE_DIR LIBGPS_LIBRARY)

if (LIBGPS_FOUND)
    set(LIBGPS_LIBRARIES ${LIBGPS_LIBRARY} )
    set(LIBGPS_INCLUDE_DIRS ${LIBGPS_INCLUDE_DIR} )
    set(LIBGPS_DEFINITIONS )
endif()

mark_as_advanced(LIBGPS_ROOT_DIR LIBGPS_INCLUDE_DIR LIBGPS_LIBRARY)