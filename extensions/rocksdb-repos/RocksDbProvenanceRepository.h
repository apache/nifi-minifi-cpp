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

#include <cinttypes>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <algorithm>
#include <utility>

#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/provenance/Provenance.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"
#include "minifi-cpp/utils/Literals.h"
#include "RocksDbRepository.h"

namespace org::apache::nifi::minifi::provenance {

constexpr auto PROVENANCE_DIRECTORY = "./provenance_repository";
constexpr auto MAX_PROVENANCE_STORAGE_SIZE = 10_MiB;
constexpr auto MAX_PROVENANCE_ENTRY_LIFE_TIME = std::chrono::minutes(1);
constexpr auto PROVENANCE_PURGE_PERIOD = std::chrono::milliseconds(2500);

class RocksDbProvenanceRepository : public core::repository::RocksDbRepository, public ProvenanceRepository {
 public:
  RocksDbProvenanceRepository(std::string_view name, const utils::Identifier& /*uuid*/)
    : RocksDbProvenanceRepository(name) {
  }

  explicit RocksDbProvenanceRepository(std::string_view repo_name = "",
      std::string directory = PROVENANCE_DIRECTORY,
      std::chrono::milliseconds maxPartitionMillis = MAX_PROVENANCE_ENTRY_LIFE_TIME,
      int64_t maxPartitionBytes = MAX_PROVENANCE_STORAGE_SIZE,
      std::chrono::milliseconds purgePeriod = PROVENANCE_PURGE_PERIOD)
    : RocksDbRepository(repo_name.length() > 0 ? repo_name : core::className<RocksDbProvenanceRepository>(),
        directory, maxPartitionMillis, maxPartitionBytes, purgePeriod, core::logging::LoggerFactory<RocksDbProvenanceRepository>::getLogger()) {
  }

  ~RocksDbProvenanceRepository() override {
    stop();
  }

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  bool isNoop() const override {
    return false;
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) override;

  bool Delete(const std::string& /*key*/) override {
    // The repo is cleaned up by itself, there is no need to delete items.
    return true;
  }

  void destroy();

  RocksDbProvenanceRepository(const RocksDbProvenanceRepository &parent) = delete;

  RocksDbProvenanceRepository &operator=(const RocksDbProvenanceRepository &parent) = delete;

  std::unique_ptr<Cursor> cursorFromString(std::string_view cursor_str) override;

  std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t max_size, Cursor* cursor) override;

  std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) override;

 private:
  // Run function for the thread
  void run() override {};

  std::unique_ptr<minifi::internal::RocksDatabase> internal_state_db_;
  std::mutex next_event_id_mtx_;
  utils::Identifier next_event_id_;
};

}  // namespace org::apache::nifi::minifi::provenance
