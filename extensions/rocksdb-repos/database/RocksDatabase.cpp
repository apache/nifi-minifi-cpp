// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "RocksDatabase.h"

#include <unordered_map>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"
#include "RocksDbInstance.h"

namespace org::apache::nifi::minifi::internal {

std::unique_ptr<RocksDatabase> RocksDatabase::create(const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch, const std::string& uri,
    const std::unordered_map<std::string, std::string>& db_config_override, RocksDbMode mode) {
  const std::string scheme = "minifidb://";

  auto logger = core::logging::LoggerFactory<RocksDatabase>::getLogger();
  logger->log_info("Acquiring database handle '{}'", uri);
  std::string db_path = uri;
  std::string db_column = "default";
  if (utils::string::startsWith(uri, scheme)) {
    const std::string path = uri.substr(scheme.length());
    logger->log_info("RocksDB scheme is detected in '{}'", uri);
    // last segment is treated as the column name
    std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) {
      pos = path.find_last_of('\\');
    }
    if (pos == std::string::npos) {
      logger->log_error("Couldn't detect the column name in '{}'", uri);
      return nullptr;
    }
    db_path = path.substr(0, pos);
    db_column = path.substr(pos + 1);
    logger->log_info("Using column '{}' in rocksdb database '{}'", db_column, db_path);
  } else {
    logger->log_info("Simple directory detected '{}', using as is", uri);
  }

  if (mode == RocksDbMode::ReadOnly) {
    // no need to cache anything with read-only databases
    return std::make_unique<RocksDatabase>(std::make_shared<RocksDbInstance>(db_path, mode), db_column, db_options_patch, cf_options_patch, db_config_override);
  }

  static std::mutex mtx;
  static std::unordered_map<std::string, std::weak_ptr<RocksDbInstance>> databases;

  std::shared_ptr<RocksDbInstance> instance;
  {
    std::lock_guard<std::mutex> guard(mtx);
    instance = databases[db_path].lock();
    if (!instance) {
      logger->log_info("Opening rocksdb database '{}'", db_path);
      instance = std::make_shared<RocksDbInstance>(db_path, mode);
      databases[db_path] = instance;
    } else {
      logger->log_info("Using previously opened rocksdb instance '{}'", db_path);
    }
  }
  return std::make_unique<RocksDatabase>(instance, db_column, db_options_patch, cf_options_patch, db_config_override);
}

RocksDatabase::RocksDatabase(std::shared_ptr<RocksDbInstance> db, std::string column, const DBOptionsPatch& db_options_patch, const ColumnFamilyOptionsPatch& cf_options_patch,
  const std::unordered_map<std::string, std::string>& db_config_override)
    : column_(std::move(column)), db_(std::move(db)) {
  db_->registerColumnConfig(column_, db_options_patch, cf_options_patch, db_config_override);
}

RocksDatabase::~RocksDatabase() {
  db_->unregisterColumnConfig(column_);
}

std::optional<OpenRocksDb> RocksDatabase::open() {
  return db_->open(column_);
}

}  // namespace org::apache::nifi::minifi::internal
