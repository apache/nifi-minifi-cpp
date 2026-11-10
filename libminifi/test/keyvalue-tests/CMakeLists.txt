# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


add_minifi_executable(PersistentMapStateStorageTest "PersistentStateStorageTest.cpp")
createTests(PersistentMapStateStorageTest)
add_test(NAME PersistentMapStateStorageTest COMMAND PersistentMapStateStorageTest --config-yaml "${TEST_RESOURCES}/PersistentMapStateStorage.yml")
target_link_libraries(PersistentMapStateStorageTest minifi-standard-processors Catch2)

add_minifi_executable(VolatileMapStateStorageTest "VolatileMapStateStorageTest.cpp")
createTests(VolatileMapStateStorageTest)
add_test(NAME VolatileMapStateStorageTest COMMAND VolatileMapStateStorageTest --config-yaml "${TEST_RESOURCES}/VolatileMapStateStorage.yml")
target_link_libraries(VolatileMapStateStorageTest minifi-standard-processors Catch2)

if (ENABLE_ROCKSDB)
    add_minifi_executable(RocksDbStateStorageTest "PersistentStateStorageTest.cpp")
    createTests(RocksDbStateStorageTest)
    add_test(NAME RocksDbStateStorageTest COMMAND RocksDbStateStorageTest --config-yaml "${TEST_RESOURCES}/RocksDbStateStorage.yml")
    target_link_libraries(RocksDbStateStorageTest minifi-rocksdb-repos Catch2)
endif()
