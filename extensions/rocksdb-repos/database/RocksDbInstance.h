// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "RocksDbUtils.h"
#include "rocksdb/db.h"
#include "minifi-cpp/core/logging/Logger.h"

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

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::internal
