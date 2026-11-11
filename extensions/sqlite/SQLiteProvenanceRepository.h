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

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/ThreadedRepository.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/provenance/Provenance.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"
#include "minifi-cpp/utils/Literals.h"

struct sqlite3;

namespace org::apache::nifi::minifi::provenance {

class SQLiteProvenanceRepository : public core::ThreadedRepositoryImpl, public ProvenanceRepository {
 public:
  static constexpr auto DefaultDirectory = "./provenance_repository";
  static constexpr int64_t DefaultMaxStorageSize = 10_MiB;
  static constexpr auto DefaultMaxEntryLifeTime = std::chrono::minutes(1);
  static constexpr auto DefaultPurgePeriod = std::chrono::milliseconds(2500);

  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;

  SQLiteProvenanceRepository(std::string_view name, const utils::Identifier& /*uuid*/)
    : SQLiteProvenanceRepository(name) {
  }

  explicit SQLiteProvenanceRepository(std::string_view repo_name = "",
      std::string directory = DefaultDirectory,
      std::chrono::milliseconds maxPartitionMillis = DefaultMaxEntryLifeTime,
      int64_t maxPartitionBytes = DefaultMaxStorageSize,
      std::chrono::milliseconds purgePeriod = DefaultPurgePeriod);

  ~SQLiteProvenanceRepository() override;

  bool initialize(const std::shared_ptr<Configure>& config) override;

  bool isNoop() const override {
    return false;
  }

  void run() override;

  std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<ProvenanceEventRecord>>& events) override;

  std::unique_ptr<Cursor> cursorFromString(std::string_view cursor_str) override;

  std::expected<std::vector<std::shared_ptr<ProvenanceEventRecord>>, std::string> getEvents(size_t max_size, Cursor* cursor) override;

  uint64_t getRepositorySize() const override;

  uint64_t getRepositoryEntryCount() const override;

 protected:
  std::thread& getThread() override {
    return thread_;
  }

 private:
  class EventCursor;

  struct DbDeleter {
    void operator()(sqlite3* db) const;
  };

  void initializeSchema();
  void purge();

  std::unique_ptr<sqlite3, DbDeleter> db_;
  std::thread thread_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::provenance
