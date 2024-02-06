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

file(GLOB KEYVALUE_TESTS  "*.cpp")

SET(KEYVALUE_INT_TEST_COUNT 0)

FOREACH(testfile ${KEYVALUE_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    createTests("${testfilename}")
    target_link_libraries(${testfilename} minifi-standard-processors Catch2)
    if (NOT DISABLE_ROCKSDB)
        target_link_libraries(${testfilename} minifi-rocksdb-repos)
    endif()

    MATH(EXPR KEYVALUE_INT_TEST_COUNT "${KEYVALUE_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${KEYVALUE_INT_TEST_COUNT} keyvalue controller test file(s)...")

add_test(NAME PersistentMapStateStorageTest COMMAND PersistentStateStorageTest --config-yaml "${TEST_RESOURCES}/PersistentMapStateStorage.yml")
if (NOT DISABLE_ROCKSDB)
    add_test(NAME RocksDbStateStorageTest COMMAND PersistentStateStorageTest --config-yaml "${TEST_RESOURCES}/RocksDbStateStorage.yml")
endif()
add_test(NAME VolatileMapStateStorageTest COMMAND VolatileMapStateStorageTest --config-yaml "${TEST_RESOURCES}/VolatileMapStateStorage.yml")
