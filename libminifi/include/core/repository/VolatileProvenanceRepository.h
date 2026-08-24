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

#include <string>
#include <string_view>

#include "VolatileRepository.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"

namespace org::apache::nifi::minifi::core::repository {

class VolatileProvenanceRepository : public VolatileRepository, public provenance::ProvenanceRepository {
 public:
  explicit VolatileProvenanceRepository(std::string_view repo_name = "",
                                        std::string /*dir*/ = REPOSITORY_DIRECTORY,
                                        std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                                        int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                                        std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
    : VolatileRepository(repo_name.length() > 0 ? repo_name : core::className<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod) {
  }

  ~VolatileProvenanceRepository() override {
    stop();
  }

  bool initialize(const std::shared_ptr<Configure> &configure) override {
    if (!VolatileRepository::initialize(configure)) {
      return false;
    }
    next_event_id_ = utils::IdGenerator::getIdGenerator()->generate();
    return true;
  }

  std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>& events) override {
    EntryStreams data;
    data.reserve(events.size());
    std::lock_guard guard(next_event_id_mtx_);
    for (auto& event : events) {
      event->setUUID(next_event_id_++);
      data.emplace_back(event->getUUIDStr(), std::make_unique<io::BufferStream>());
      event->serialize(*data.back().second);
    }
    MultiPut(data);

    return {};
  }

  std::unique_ptr<ProvenanceRepository::Cursor> cursorFromString(std::string_view /*cursor_str*/) override {
    return nullptr;
  }

  std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t /*max_size*/, Cursor* /*cursor*/) override {
    return std::unexpected{"Querying events is not yet supported"};
  }

 private:
  void run() override {
  }

  std::thread& getThread() override {
    return thread_;
  }

  void emplace(RepoValue<std::string> &old_value) override {
    purge_list_.push_back(old_value.getKey());
  }

  std::thread thread_;
  std::mutex next_event_id_mtx_;
  utils::Identifier next_event_id_;
};

}  // namespace org::apache::nifi::minifi::core::repository
