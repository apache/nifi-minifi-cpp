// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <unordered_map>
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "../database/RocksDbUtils.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Get global RocksDB options to override", "[rocksdbutils]") {
  std::unordered_map<std::string, std::string> expected_options {
    {"keep_log_file_num", "2"},
    {"table_cache_numshardbits", "456"},
    {"create_if_missing", "false"}
  };

  auto config = std::make_shared<minifi::ConfigureImpl>();
  config->set("nifi.c2.rest.url", "http://localhost:8080");
  config->set("nifi.global.rocksdb.options.table_cache_numshardbits", "456");
  config->set("nifi.global.rocksdb.options.keep_log_file_num", "123");
  config->set("nifi.flowfile.repository.rocksdb.options.keep_log_file_num", "2");
  config->set("nifi.flowfile.repository.rocksdb.options.create_if_missing", "false");
  config->set("nifi.c2.enable", "true");
  auto result = org::apache::nifi::minifi::internal::getRocksDbOptionsToOverride(config, minifi::Configure::nifi_flowfile_repository_rocksdb_options);
  REQUIRE(result == expected_options);
}

}  // namespace org::apache::nifi::minifi::test
