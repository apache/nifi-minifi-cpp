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

#include <cinttypes>
#include <fstream>
#include <utility>

#include "RocksDbStateStorage.h"
#include "../encryption/RocksDbEncryptionProvider.h"
#include "utils/StringUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

RocksDbStateStorage::RocksDbStateStorage(const std::string& name, const utils::Identifier& uuid /*= utils::Identifier()*/)
    : KeyValueStateStorage(name, uuid) {
}

RocksDbStateStorage::~RocksDbStateStorage() {
  auto_persistor_.stop();
}

void RocksDbStateStorage::initialize() {
  ControllerServiceImpl::initialize();
  setSupportedProperties(Properties);
}

void RocksDbStateStorage::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable RocksDbStateStorage");
    return;
  }

  const auto always_persist = getProperty<bool>(AlwaysPersist).value_or(false);
  logger_->log_info("Always Persist property: {}", always_persist);

  const auto auto_persistence_interval = getProperty<core::TimePeriodValue>(AutoPersistenceInterval).value_or(core::TimePeriodValue{}).getMilliseconds();
  logger_->log_info("Auto Persistence Interval property: {}", auto_persistence_interval);

  if (!getProperty(Directory, directory_)) {
    logger_->log_error("Invalid or missing property: Directory");
    return;
  }

  auto_persistor_.start(always_persist, auto_persistence_interval, [this] { return persistNonVirtual(); });

  db_.reset();

  auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configuration_->getHome()}, core::repository::DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
  if (!encrypted_env) {
    encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configuration_->getHome()}, core::repository::DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME_OLD});
  }
  logger_->log_info("Using {} RocksDbStateStorage", encrypted_env ? "encrypted" : "plaintext");

  auto set_db_opts = [encrypted_env] (internal::Writable<rocksdb::DBOptions>& db_opts) {
    minifi::internal::setCommonRocksDbOptions(db_opts);
    if (encrypted_env) {
      db_opts.set(&rocksdb::DBOptions::env, encrypted_env.get(), core::repository::EncryptionEq{});
    } else {
      db_opts.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
    }
  };
  // Use the same buffer settings as the FlowFileRepository
  auto set_cf_opts = [] (rocksdb::ColumnFamilyOptions& cf_opts) {
    cf_opts.write_buffer_size = 8ULL << 20U;
    cf_opts.min_write_buffer_number_to_merge = 1;
  };
  db_ = minifi::internal::RocksDatabase::create(set_db_opts, set_cf_opts, directory_,
    minifi::internal::getRocksDbOptionsToOverride(configuration_, Configure::nifi_state_storage_rocksdb_options));
  if (db_->open()) {
    logger_->log_trace("Successfully opened RocksDB database at {}", directory_.c_str());
  } else {
    // TODO(adebreceni) forward the status
    logger_->log_error("Failed to open RocksDB database at {}, error", directory_.c_str());
    return;
  }

  if (auto_persistor_.isAlwaysPersisting()) {
    default_write_options.sync = true;
  }

  logger_->log_trace("Enabled RocksDbStateStorage");
}

void RocksDbStateStorage::notifyStop() {
  auto_persistor_.stop();
  db_.reset();
}

bool RocksDbStateStorage::set(const std::string& key, const std::string& value) {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Status status = opendb->Put(default_write_options, key, value);
  if (!status.ok()) {
    logger_->log_error("Failed to Put key {} to RocksDB database at {}, error: {}", key.c_str(), directory_.c_str(), status.getState());
    return false;
  }
  return true;
}

bool RocksDbStateStorage::get(const std::string& key, std::string& value) {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Status status = opendb->Get(rocksdb::ReadOptions(), key, &value);
  if (!status.ok()) {
    if (status.getState() != nullptr) {
      logger_->log_error("Failed to Get key {} from RocksDB database at {}, error: {}", key.c_str(), directory_.c_str(), status.getState());
    } else {
      logger_->log_warn("Failed to Get key {} from RocksDB database at {} (it may not have been initialized yet)", key.c_str(), directory_.c_str());
    }
    return false;
  }
  return true;
}

bool RocksDbStateStorage::get(std::unordered_map<std::string, std::string>& kvs) {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  kvs.clear();
  auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    kvs.emplace(it->key().ToString(), it->value().ToString());
  }
  if (!it->status().ok()) {
    logger_->log_error("Encountered error when iterating through RocksDB database at {}, error: {}", directory_.c_str(), it->status().getState());
    return false;
  }
  return true;
}

bool RocksDbStateStorage::remove(const std::string& key) {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Status status = opendb->Delete(default_write_options, key);
  if (!status.ok()) {
    logger_->log_error("Failed to Delete from RocksDB database at {}, error: {}", directory_.c_str(), status.getState());
    return false;
  }
  return true;
}

bool RocksDbStateStorage::clear() {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    rocksdb::Status status = opendb->Delete(default_write_options, it->key());
    if (!status.ok()) {
      logger_->log_error("Failed to Delete from RocksDB database at {}, error: {}", directory_.c_str(), status.getState());
      return false;
    }
  }
  if (!it->status().ok()) {
    logger_->log_error("Encountered error when iterating through RocksDB database at {}, error: {}", directory_.c_str(), it->status().getState());
    return false;
  }
  return true;
}

bool RocksDbStateStorage::update(const std::string& /*key*/, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& /*update_func*/) {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  throw std::logic_error("Unsupported method");
}

bool RocksDbStateStorage::persistNonVirtual() {
  if (!db_) {
    return false;
  }
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  if (auto_persistor_.isAlwaysPersisting()) {
    return true;
  }
  return opendb->FlushWAL(true /*sync*/).ok();
}

REGISTER_RESOURCE_AS(RocksDbStateStorage, ControllerService, ("RocksDbPersistableKeyValueStoreService", "rocksdbpersistablekeyvaluestoreservice", "RocksDbStateStorage"));

}  // namespace org::apache::nifi::minifi::controllers
