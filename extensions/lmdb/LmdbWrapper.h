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
