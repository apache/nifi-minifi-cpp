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

#include "LmdbWrapper.h"

#include <filesystem>

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::lmdb {

bool LmdbWrapper::initialize(const std::string& directory, size_t max_db_size) {
  if (const auto rc = mdb_env_create(&lmdb_env_)) {
    logger_->log_error("Failed to create LMDB environment: {}", mdb_strerror(rc));
    return false;
  }

  logger_->log_info("Setting LMDB max DB size to {} bytes", max_db_size);
  if (const auto rc = mdb_env_set_mapsize(lmdb_env_, max_db_size); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to set LMDB map size: {}", mdb_strerror(rc));
    mdb_env_close(lmdb_env_);
    lmdb_env_ = nullptr;
    return false;
  }

  if (std::filesystem::exists(directory)) {
    logger_->log_info("Using existing LMDB Repository directory at {}", directory);
  } else {
    logger_->log_info("Creating LMDB Repository directory at {}", directory);
    if (!std::filesystem::create_directories(directory)) {
      logger_->log_error("Failed to create LMDB Repository directory at {}", directory);
      return false;
    }
  }

  if (const auto rc = mdb_env_open(lmdb_env_, directory.c_str(), MDB_NOTLS, 0664)) {
    logger_->log_error("Failed to open LMDB environment: {}", mdb_strerror(rc));
    mdb_env_close(lmdb_env_);
    lmdb_env_ = nullptr;
    return false;
  }

  MDB_txn* init_txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &init_txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB transaction during initialize: {}", mdb_strerror(rc));
    mdb_env_close(lmdb_env_);
    lmdb_env_ = nullptr;
    return false;
  }
  if (const auto rc = mdb_dbi_open(init_txn, nullptr, 0, &lmdb_handle_); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to open LMDB database: {}", mdb_strerror(rc));
    mdb_txn_abort(init_txn);
    mdb_env_close(lmdb_env_);
    lmdb_env_ = nullptr;
    return false;
  }

  if (const auto rc = mdb_txn_commit(init_txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB transaction during initialize: {}", mdb_strerror(rc));
    mdb_env_close(lmdb_env_);
    lmdb_env_ = nullptr;
    return false;
  }

  return true;
}

bool LmdbWrapper::exists(const std::string& path) const {
  MDB_val key{path.size(), const_cast<char*>(path.data())};
  MDB_val value{};

  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB read transaction in exists: {}", mdb_strerror(rc));
    return false;
  }
  auto guard = gsl::finally([txn] { mdb_txn_abort(txn); });

  const auto rc = mdb_get(txn, lmdb_handle_, &key, &value);
  if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND) { logger_->log_error("Failed to get value from LMDB database: {}", mdb_strerror(rc)); }
  return rc == MDB_SUCCESS;
}

bool LmdbWrapper::removeKey(const std::string& path) {
  MDB_val key{path.size(), const_cast<char*>(path.data())};

  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB write transaction in removeKey: {}", mdb_strerror(rc));
    return false;
  }
  const auto rc = mdb_del(txn, lmdb_handle_, &key, nullptr);

  if (rc == MDB_SUCCESS) {
    if (const auto rc = mdb_txn_commit(txn); rc != MDB_SUCCESS) {
      logger_->log_error("Failed to commit LMDB transaction during delete: {}", mdb_strerror(rc));
      return false;
    }
    return true;
  } else if (rc == MDB_NOTFOUND) {
    logger_->log_debug("Key {} not found in LMDB database during delete", path);
    mdb_txn_abort(txn);
    return true;
  } else {
    logger_->log_error("Failed to delete key '{}' from LMDB database: {}", path, mdb_strerror(rc));
    mdb_txn_abort(txn);
    return false;
  }
}

bool LmdbWrapper::removeKeys(const std::vector<std::string>& paths) {
  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB write transaction in removeKeys: {}", mdb_strerror(rc));
    return false;
  }

  for (const auto& key_str : paths) {
    MDB_val key{key_str.size(), const_cast<char*>(key_str.data())};
    auto rc = mdb_del(txn, lmdb_handle_, &key, nullptr);

    if (rc == MDB_NOTFOUND) {
      logger_->log_warn("Key {} not found in LMDB database during delete", key_str);
    } else if (rc != MDB_SUCCESS) {
      logger_->log_error("Failed to delete key '{}' from LMDB database: {}", key_str, mdb_strerror(rc));
      mdb_txn_abort(txn);
      return false;
    }
  }

  if (const auto rc = mdb_txn_commit(txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB transaction during delete: {}", mdb_strerror(rc));
    return false;
  }
  return true;
}

bool LmdbWrapper::forEach(const std::function<void(const MDB_val& key, const MDB_val& value)>& func) const {
  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB read transaction in forEach: {}", mdb_strerror(rc));
    return false;
  }

  auto txn_guard = gsl::finally([txn] { mdb_txn_abort(txn); });

  MDB_cursor* cursor = nullptr;
  if (const auto rc = mdb_cursor_open(txn, lmdb_handle_, &cursor); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to open LMDB cursor in forEach: {}", mdb_strerror(rc));
    return false;
  }
  auto cursor_guard = gsl::finally([cursor] { mdb_cursor_close(cursor); });

  MDB_val key{};
  MDB_val val{};
  auto rc = mdb_cursor_get(cursor, &key, &val, MDB_FIRST);

  while (rc == MDB_SUCCESS) {
    func(key, val);
    rc = mdb_cursor_get(cursor, &key, &val, MDB_NEXT);
  }

  if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to iterate over LMDB database: {}", mdb_strerror(rc));
    return false;
  }
  return true;
}

std::optional<std::string> LmdbWrapper::getValue(const std::string& path) const {
  MDB_val key{path.size(), const_cast<char*>(path.data())};
  MDB_val value{};

  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB read transaction in getValue: {}", mdb_strerror(rc));
    return std::nullopt;
  }
  auto guard = gsl::finally([txn] { mdb_txn_abort(txn); });

  const auto rc = mdb_get(txn, lmdb_handle_, &key, &value);
  if (rc == MDB_SUCCESS) {
    return std::string(static_cast<char*>(value.mv_data), value.mv_size);
  } else if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to get key '{}' from LMDB database: {}", path, mdb_strerror(rc));
  }
  return std::nullopt;
}

bool LmdbWrapper::putValue(const std::string& path, const std::string& value) {
  MDB_txn* txn = nullptr;
  auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB transaction in putValue: {}", mdb_strerror(rc));
    return false;
  }

  MDB_val key{path.size(), const_cast<char*>(path.data())};
  MDB_val val{value.size(), const_cast<char*>(value.data())};
  rc = mdb_put(txn, lmdb_handle_, &key, &val, 0);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to put value in LMDB database during putValue: {}", mdb_strerror(rc));
    mdb_txn_abort(txn);
    return false;
  }

  rc = mdb_txn_commit(txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB transaction during putValue: {}", mdb_strerror(rc));
    return false;
  }
  return true;
}

bool LmdbWrapper::putValues(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB write transaction in putValues: {}", mdb_strerror(rc));
    return false;
  }
  for (const auto& item : data) {
    const auto buf = item.second->getBuffer();
    MDB_val mdb_key{item.first.size(), const_cast<char*>(item.first.data())};
    MDB_val mdb_value{buf.size(), const_cast<std::byte*>(buf.data())};
    if (const auto rc = mdb_put(txn, lmdb_handle_, &mdb_key, &mdb_value, 0); rc != MDB_SUCCESS) {
      logger_->log_error("Failed to put key '{}' into LMDB during putValues: {}", item.first, mdb_strerror(rc));
      mdb_txn_abort(txn);
      return false;
    }
  }
  if (const auto rc = mdb_txn_commit(txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB putValues transaction: {}", mdb_strerror(rc));
    return false;
  }
  return true;
}

MDB_stat LmdbWrapper::getDbStat() const {
  MDB_stat stat{};
  MDB_txn* txn = nullptr;
  if (const auto rc = mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB read transaction in getDbStat: {}", mdb_strerror(rc));
    return stat;
  }
  if (const auto rc = mdb_stat(txn, lmdb_handle_, &stat); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to read LMDB database stats: {}", mdb_strerror(rc));
  }
  mdb_txn_abort(txn);
  return stat;
}

}  // namespace org::apache::nifi::minifi::extensions::lmdb
