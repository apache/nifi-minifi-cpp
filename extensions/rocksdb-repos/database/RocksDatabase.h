// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "rocksdb/db.h"
#include "minifi-cpp/core/logging/Logger.h"
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
};

}  // namespace org::apache::nifi::minifi::internal
