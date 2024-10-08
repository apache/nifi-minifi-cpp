/**
 *
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

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "rocksdb/db.h"
#include "core/logging/Logger.h"
#include "RocksDbUtils.h"
#include "OpenRocksDb.h"

namespace org::apache::nifi::minifi::internal {

class RocksDbInstance;

/**
 * Purpose: represents a column in a RocksDbInstance.
 * There can be multiple RocksDatabase handles to the same instance.
 */
class RocksDatabase {
 public:
  static std::unique_ptr<RocksDatabase> create(const DBOptionsPatch& db_options_patch,
                                               const ColumnFamilyOptionsPatch& cf_options_patch,
                                               const std::string& uri,
                                               const std::unordered_map<std::string, std::string>& db_config_override,
                                               RocksDbMode mode = RocksDbMode::ReadWrite);

  RocksDatabase(std::shared_ptr<RocksDbInstance> db, std::string column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch,
    const std::unordered_map<std::string, std::string>& db_config_override);

  RocksDatabase(const RocksDatabase&) = delete;
  RocksDatabase(RocksDatabase&&) = delete;
  RocksDatabase& operator=(const RocksDatabase&) = delete;
  RocksDatabase& operator=(RocksDatabase&&) = delete;

  std::optional<OpenRocksDb> open();

  ~RocksDatabase();

 private:
  const std::string column_;
  std::shared_ptr<RocksDbInstance> db_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::internal
