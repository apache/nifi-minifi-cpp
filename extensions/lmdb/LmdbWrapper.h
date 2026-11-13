// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/logging/LoggerFactory.h"
#include "io/BufferStream.h"
#include "lmdb.h"

namespace org::apache::nifi::minifi::extensions::lmdb {

class LmdbWrapper {
 public:
  LmdbWrapper() = default;
  ~LmdbWrapper() {
    if (lmdb_env_) {
      mdb_dbi_close(lmdb_env_, lmdb_handle_);
      mdb_env_close(lmdb_env_);
    }
  }

  LmdbWrapper(const LmdbWrapper&) = delete;
  LmdbWrapper& operator=(const LmdbWrapper&) = delete;
  LmdbWrapper(LmdbWrapper&&) = delete;
  LmdbWrapper& operator=(LmdbWrapper&&) = delete;

  bool initialize(const std::string& directory, size_t max_db_size);
  bool exists(const std::string& path) const;
  MDB_stat getDbStat() const;
  bool removeKey(const std::string& path);
  bool removeKeys(const std::vector<std::string>& paths);
  bool forEach(const std::function<void(const MDB_val& key, const MDB_val& value)>& func) const;
  std::optional<std::string> getValue(const std::string& path) const;
  bool putValue(const std::string& path, const std::string& value);
  bool putValues(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data);

 private:
  MDB_env* lmdb_env_{nullptr};
  MDB_dbi lmdb_handle_{};
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<LmdbWrapper>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::extensions::lmdb
