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
#include "utils/Literals.h"
#include "RocksDbRepository.h"

namespace org::apache::nifi::minifi::provenance {

constexpr auto PROVENANCE_DIRECTORY = "./provenance_repository";
constexpr auto MAX_PROVENANCE_STORAGE_SIZE = 10_MiB;
constexpr auto MAX_PROVENANCE_ENTRY_LIFE_TIME = std::chrono::minutes(1);
constexpr auto PROVENANCE_PURGE_PERIOD = std::chrono::milliseconds(2500);

class ProvenanceRepository : public core::repository::RocksDbRepository {
 public:
  ProvenanceRepository(std::string_view name, const utils::Identifier& /*uuid*/)
    : ProvenanceRepository(name) {
  }

  explicit ProvenanceRepository(std::string_view repo_name = "",
                                std::string directory = PROVENANCE_DIRECTORY,
                                std::chrono::milliseconds maxPartitionMillis = MAX_PROVENANCE_ENTRY_LIFE_TIME,
                                int64_t maxPartitionBytes = MAX_PROVENANCE_STORAGE_SIZE,
                                std::chrono::milliseconds purgePeriod = PROVENANCE_PURGE_PERIOD)
    : RocksDbRepository(repo_name.length() > 0 ? repo_name : core::className<ProvenanceRepository>(),
        directory, maxPartitionMillis, maxPartitionBytes, purgePeriod, core::logging::LoggerFactory<ProvenanceRepository>::getLogger()) {
  }

  ~ProvenanceRepository() override {
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
  bool getElements(std::vector<std::shared_ptr<core::SerializableComponent>> &records, size_t &max_size) override;

  void destroy();

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceRepository(const ProvenanceRepository &parent) = delete;

  ProvenanceRepository &operator=(const ProvenanceRepository &parent) = delete;

 private:
  // Run function for the thread
  void run() override {};
};

}  // namespace org::apache::nifi::minifi::provenance
