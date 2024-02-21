/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unordered_map>
#include <string>

#include "TestBase.h"
#include "Catch.h"
#include "../database/RocksDbUtils.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Get global RocksDB options to override", "[rocksdbutils]") {
  std::unordered_map<std::string, std::string> expected_options {
    {"keep_log_file_num", "123"},
    {"table_cache_numshardbits", "456"},
    {"create_if_missing", "true"}
  };

  auto config = std::make_shared<minifi::Configure>();
  config->set("nifi.c2.rest.url", "http://localhost:8080");
  config->set("nifi.global.rocksdb.options.table_cache_numshardbits", "456");
  config->set("nifi.global.rocksdb.options.keep_log_file_num", "123");
  config->set("nifi.global.rocksdb.options.create_if_missing", "true");
  config->set("nifi.c2.enable", "true");
  auto result = org::apache::nifi::minifi::internal::getGlobalRocksDbOptionsToOverride(config);
  REQUIRE(result == expected_options);
}

}  // namespace org::apache::nifi::minifi::test
