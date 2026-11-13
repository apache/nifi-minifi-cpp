// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
    next_event_key_ =  1;
    return true;
  }

  std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>& events) override {
    EntryStreams data;
    data.reserve(events.size());
    std::lock_guard guard(next_event_key_mtx_);
    for (auto& event : events) {
      event->setEventOrdinal(next_event_key_++);
      data.emplace_back(std::to_string(event->getEventOrdinal()), std::make_unique<io::BufferStream>());
      event->serialize(*data.back().second);
    }
    MultiPut(data);

    return {};
  }

  std::unique_ptr<ProvenanceRepository::Cursor> cursorFromString(std::string_view cursor_str) override;

  // Returns the events following the cursor in ordinal order. As this repository can overwrite
  // its oldest entries, a cursor guarantees that no event is returned twice, not that no event
  // is missed.
  std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t max_size, Cursor* cursor) override;

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
  std::mutex next_event_key_mtx_;
  uint64_t next_event_key_;
};

}  // namespace org::apache::nifi::minifi::core::repository
