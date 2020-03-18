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

#include "RocksDbPersistableKeyValueStoreService.h"

#include "utils/StringUtils.h"

#include <fstream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property RocksDbPersistableKeyValueStoreService::Directory(
    core::PropertyBuilder::createProperty("Directory")->withDescription("Path to a directory for the database")
        ->isRequired(true)->build());

RocksDbPersistableKeyValueStoreService::RocksDbPersistableKeyValueStoreService(const std::string& name, const std::string& id)
    : controllers::KeyValueStoreService(name, id)
    , controllers::AbstractAutoPersistingKeyValueStoreService(name, id)
    , logger_(logging::LoggerFactory<RocksDbPersistableKeyValueStoreService>::getLogger()) {
}

RocksDbPersistableKeyValueStoreService::RocksDbPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : controllers::KeyValueStoreService(name, uuid)
    , controllers::AbstractAutoPersistingKeyValueStoreService(name, uuid)
    , logger_(logging::LoggerFactory<RocksDbPersistableKeyValueStoreService>::getLogger()) {
}

void RocksDbPersistableKeyValueStoreService::initialize() {
  AbstractAutoPersistingKeyValueStoreService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(Directory);
  updateSupportedProperties(supportedProperties);
}

void RocksDbPersistableKeyValueStoreService::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable RocksDbPersistableKeyValueStoreService");
    return;
  }

  AbstractAutoPersistingKeyValueStoreService::onEnable();

  if (!getProperty(Directory.getName(), directory_)) {
    logger_->log_error("Invalid or missing property: Directory");
    return;
  }

  db_.reset();
  rocksdb::Options options;
  options.create_if_missing = true;
  if (!always_persist_) {
    options.manual_wal_flush = true;
  }
  rocksdb::DB* db = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(options, directory_, &db);
  if (status.ok()) {
    db_.reset(db);
    logger_->log_trace("Successfully opened RocksDB database at %s", directory_.c_str());
  } else {
    logger_->log_error("Failed to open RocksDB database at %s, error: %s", directory_.c_str(), status.getState());
    return;
  }

  if (always_persist_) {
    default_write_options.sync = true;
  }

  logger_->log_trace("Enabled RocksDbPersistableKeyValueStoreService");
}

void RocksDbPersistableKeyValueStoreService::notifyStop() {
  AbstractAutoPersistingKeyValueStoreService::notifyStop();

  db_.reset();
}

bool RocksDbPersistableKeyValueStoreService::set(const std::string& key, const std::string& value) {
  if (!db_) {
    return false;
  }
  rocksdb::Status status = db_->Put(default_write_options, key, value);
  if (!status.ok()) {
    logger_->log_error("Failed to Put key %s to RocksDB database at %s, error: %s", key.c_str(), directory_.c_str(), status.getState());
    return false;
  }
  return true;
}

bool RocksDbPersistableKeyValueStoreService::get(const std::string& key, std::string& value) {
  if (!db_) {
    return false;
  }
  rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), key, &value);
  if (!status.ok()) {
    logger_->log_error("Failed to Get key %s from RocksDB database at %s, error: %s", key.c_str(), directory_.c_str(), status.getState());
    return false;
  }
  return true;
}

bool RocksDbPersistableKeyValueStoreService::get(std::unordered_map<std::string, std::string>& kvs) {
  if (!db_) {
    return false;
  }
  kvs.clear();
  std::unique_ptr<rocksdb::Iterator> it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(rocksdb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    kvs.emplace(it->key().ToString(), it->value().ToString());
  }
  if (!it->status().ok()) {
    logger_->log_error("Encountered error when iterating through RocksDB database at %s, error: %s", directory_.c_str(), it->status().getState());
    return false;
  }
  return true;
}

bool RocksDbPersistableKeyValueStoreService::remove(const std::string& key) {
  if (!db_) {
    return false;
  }
  rocksdb::Status status = db_->Delete(default_write_options, key);
  if (!status.ok()) {
    logger_->log_error("Failed to Delete from RocksDB database at %s, error: %s", directory_.c_str(), status.getState());
    return false;
  }
  return true;
}

bool RocksDbPersistableKeyValueStoreService::clear() {
  if (!db_) {
    return false;
  }
  std::unique_ptr<rocksdb::Iterator> it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(rocksdb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    rocksdb::Status status = db_->Delete(default_write_options, it->key());
    if (!status.ok()) {
      logger_->log_error("Failed to Delete from RocksDB database at %s, error: %s", directory_.c_str(), status.getState());
      return false;
    }
  }
  if (!it->status().ok()) {
    logger_->log_error("Encountered error when iterating through RocksDB database at %s, error: %s", directory_.c_str(), it->status().getState());
    return false;
  }
  return true;
}

bool RocksDbPersistableKeyValueStoreService::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  if (!db_) {
    return false;
  }
  throw std::logic_error("Unsupported method");
}

bool RocksDbPersistableKeyValueStoreService::persist() {
  if (!db_) {
    return false;
  }
  if (always_persist_) {
    return true;
  }
  return db_->FlushWAL(true /*sync*/).ok();
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
