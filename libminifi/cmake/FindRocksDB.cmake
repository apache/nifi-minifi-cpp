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

#  ROCKSDB_FOUND               System has RocksDB library/headers.
#  ROCKSDB_LIBRARIES           The RocksDB library.
#  ROCKSDB_INCLUDE_DIR        The location of RocksDB headers.

find_path(ROCKSDB_ROOT_DIR
    NAMES include/rocksdb/db.h
)

find_library(ROCKSDB_LIBRARIES
    NAMES rocksdb
    HINTS ${ROCKSDB_ROOT_DIR}/lib
)

find_path(ROCKSDB_INCLUDE_DIR
    NAMES rocksdb/db.h
    HINTS ${ROCKSDB_ROOT_DIR}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RocksDB DEFAULT_MSG
    ROCKSDB_LIBRARIES
    ROCKSDB_INCLUDE_DIR
)

mark_as_advanced(
    ROCKSDB_ROOT_DIR
    ROCKSDB_LIBRARIES
    ROCKSDB_INCLUDE_DIR
)

if(ROCKSDB_INCLUDE_DIR AND ROCKSDB_LIBRARIES)
  set(ROCKSDB_FOUND "YES")
  message(STATUS "Found RocksDB...${ROCKSDB_LIBRARIES}")
endif()