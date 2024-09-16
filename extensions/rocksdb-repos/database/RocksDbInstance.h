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
#include "RocksDbUtils.h"
#include "rocksdb/db.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::internal {

class RocksDatabase;
class OpenRocksDb;
struct ColumnHandle;
struct DbHandle;

/**
 * Purpose: represents a single rocksdb instance backed by a directory
 */
class RocksDbInstance {
  friend class OpenRocksDb;
  friend class RocksDatabase;

  struct ColumnConfig {
    DBOptionsPatch dbo_patch;
    ColumnFamilyOptionsPatch cfo_patch;
  };

 public:
  explicit RocksDbInstance(std::string path, RocksDbMode mode = RocksDbMode::ReadWrite);

  std::optional<OpenRocksDb> open(const std::string& column);

 protected:
  // caller must hold the mtx_ mutex
  std::shared_ptr<ColumnHandle> getOrCreateColumnFamily(const std::string& column, const std::lock_guard<std::mutex>& guard);
  /*
   * notify RocksDatabase that the next open should check if they can reopen the database
   * until a successful reopen no more open is possible
   */
  void invalidate();

  void invalidate(const std::lock_guard<std::mutex>&);

  void registerColumnConfig(const std::string& column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch,
    const std::unordered_map<std::string, std::string>& db_config_override);
  void unregisterColumnConfig(const std::string& column);

  rocksdb::DBOptions db_options_;
  const std::string db_name_;
  std::unordered_map<std::string, std::string> db_config_override_;
  const RocksDbMode mode_;

  std::mutex mtx_;
  std::shared_ptr<DbHandle> impl_;
  std::unordered_map<std::string, std::shared_ptr<ColumnHandle>> columns_;

  std::unordered_map<std::string, ColumnConfig> column_configs_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::internal
