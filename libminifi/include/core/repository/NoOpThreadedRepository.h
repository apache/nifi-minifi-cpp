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

#include <string_view>
#include <thread>

#include "core/ThreadedRepository.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"

namespace org::apache::nifi::minifi::core::repository {

class NoOpThreadedRepository : public core::ThreadedRepositoryImpl, public provenance::ProvenanceRepository {
 public:
  explicit NoOpThreadedRepository(std::string_view repo_name)
    : ThreadedRepositoryImpl(repo_name) {
  }

  NoOpThreadedRepository(NoOpThreadedRepository&&) = delete;
  NoOpThreadedRepository(const NoOpThreadedRepository&) = delete;
  NoOpThreadedRepository& operator=(NoOpThreadedRepository&&) = delete;
  NoOpThreadedRepository& operator=(const NoOpThreadedRepository&) = delete;

  ~NoOpThreadedRepository() override {
    stop();
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }

  std::expected<void, std::string> appendEvents(const std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>& /*events*/) override {
    return {};
  }

  std::unique_ptr<ProvenanceRepository::Cursor> cursorFromString(std::string_view /*cursor_str*/) override {
    return nullptr;
  }

  std::expected<std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>>, std::string> getEvents(size_t /*max_size*/, Cursor* cursor) override {
    if (cursor) {
      return std::unexpected{"Cursor based query is not supported"};
    }
    return {};
  }

 private:
  void run() override {
  }

  std::thread& getThread() override {
    return thread_;
  }

  std::thread thread_;
};

using VolatileFlowFileRepository = NoOpThreadedRepository;

}  // namespace org::apache::nifi::minifi::core::repository
