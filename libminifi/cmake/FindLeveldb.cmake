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


# Find module for Leveldb library and includes
#  LEVELDB_FOUND - if system found LEVELDB library
#  LEVELDB_INCLUDE_DIRS - The LEVELDB include directories
#  LEVELDB_LIBRARIES - The libraries needed to use LEVELDB
#  LEVELDB_DEFINITIONS - Compiler switches required for using LEVELDB

# For OS X do not attempt to use the OS X application frameworks or bundles.
set (CMAKE_FIND_FRAMEWORK NEVER)
set (CMAKE_FIND_APPBUNDLE NEVER)

find_path(LEVELDB_INCLUDE_DIR
    NAMES leveldb/db.h
    PATHS /usr/local/include /usr/include
    DOC "LevelDB include header"
)

find_library(LEVELDB_LIBRARY 
    NAMES libleveldb.dylib libleveldb.so
    PATHS /usr/local/lib /usr/lib/x86_64-linux-gnu
    DOC "LevelDB library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LEVELDB DEFAULT_MSG LEVELDB_INCLUDE_DIR LEVELDB_LIBRARY)

if (LEVELDB_FOUND)
    set(LEVELDB_LIBRARIES ${LEVELDB_LIBRARY} )
    set(LEVELDB_INCLUDE_DIRS ${LEVELDB_INCLUDE_DIR} )
    set(LEVELDB_DEFINITIONS )
endif()

mark_as_advanced(LEVELDB_ROOT_DIR LEVELDB_INCLUDE_DIR LEVELDB_LIBRARY)