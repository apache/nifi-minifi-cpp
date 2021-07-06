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

#include <string>
#include <memory>
#include <unordered_map>
#include "utils/OptionalUtils.h"
#include "RocksDbUtils.h"
#include "rocksdb/db.h"
#include "logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

class OpenRocksDb;
struct ColumnHandle;

/**
 * Purpose: represents a single rocksdb instance backed by a directory
 */
class RocksDbInstance {
  friend class OpenRocksDb;

 public:
  explicit RocksDbInstance(const std::string& path, RocksDbMode mode = RocksDbMode::ReadWrite);

  utils::optional<OpenRocksDb> open(const std::string& column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch);

 protected:
  // caller must hold the mtx_ mutex
  std::shared_ptr<ColumnHandle> getOrCreateColumnFamily(const std::string& column, const ColumnFamilyOptionsPatch& cf_options_patch, const std::lock_guard<std::mutex>& guard);
  /*
   * notify RocksDatabase that the next open should check if they can reopen the database
   * until a successful reopen no more open is possible
   */
  void invalidate();

  rocksdb::DBOptions db_options_;
  const std::string db_name_;
  const RocksDbMode mode_;

  std::mutex mtx_;
  std::shared_ptr<rocksdb::DB> impl_;
  std::unordered_map<std::string, std::shared_ptr<ColumnHandle>> columns_;

  // the patcher could have internal resources the we need to keep alive
  // as long as the database is open (e.g. custom environment)
  DBOptionsPatch db_options_patch_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace internal
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
