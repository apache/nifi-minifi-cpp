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

#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "core/repository/VolatileContentRepository.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "core/Processor.h"
#include "core/ThreadedRepository.h"
#include "Connection.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "provenance/Provenance.h"
#include "SwapManager.h"
#include "io/BufferStream.h"

using namespace std::literals::chrono_literals;

const int64_t TEST_MAX_REPOSITORY_STORAGE_SIZE = 100;

template <typename T_BaseRepository>
class TestRepositoryBase : public T_BaseRepository {
 public:
  TestRepositoryBase()
    : T_BaseRepository("repo_name", "./dir", 1s, TEST_MAX_REPOSITORY_STORAGE_SIZE, 0ms) {
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &) override {
    return true;
  }

  bool isNoop() const override {
    return false;
  }

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.emplace(key, std::string{reinterpret_cast<const char*>(buf), bufLen});
    return true;
  }

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<org::apache::nifi::minifi::io::BufferStream>>>& data) override {
    for (const auto& item : data) {
      if (!Put(item.first, reinterpret_cast<const uint8_t*>(item.second->getBuffer().data()), item.second->size())) {
        return false;
      }
    }
    return true;
  }

  bool Delete(const std::string& key) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.erase(key);
    return true;
  }

  bool Get(const std::string &key, std::string &value) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    auto result = repository_results_.find(key);
    if (result != repository_results_.end()) {
      value = result->second;
      return true;
    } else {
      return false;
    }
  }

  bool getElements(std::vector<std::shared_ptr<org::apache::nifi::minifi::core::SerializableComponent>> &store, size_t &max_size) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    max_size = 0;
    for (const auto &entry : repository_results_) {
      if (max_size >= store.size()) {
        break;
      }
      const auto eventRead = store.at(max_size);
      org::apache::nifi::minifi::io::BufferStream stream(gsl::make_span(entry.second).template as_span<const std::byte>());
      eventRead->deserialize(stream);
      ++max_size;
    }
    return true;
  }

  std::map<std::string, std::string> getRepoMap() const {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    return repository_results_;
  }

 protected:
  mutable std::mutex repository_results_mutex_;
  std::map<std::string, std::string> repository_results_;
};

class TestRepository : public TestRepositoryBase<org::apache::nifi::minifi::core::RepositoryImpl> {
 public:
  bool start() override {
    return true;
  }

  bool stop() override {
    return true;
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }
};

class TestVolatileRepository : public TestRepositoryBase<org::apache::nifi::minifi::core::repository::VolatileFlowFileRepository> {
 public:
  bool start() override {
    return true;
  }

  bool stop() override {
    return true;
  }

  void setFull() {
    repo_data_.current_size = repo_data_.max_size;
    repo_data_.current_entry_count = repo_data_.max_count;
  }
};

class TestThreadedRepository : public TestRepositoryBase<org::apache::nifi::minifi::core::ThreadedRepositoryImpl> {
 public:
  ~TestThreadedRepository() override {
    stop();
  }

 private:
  void run() override {
    // do nothing
  }

  std::thread& getThread() override {
    return thread_;
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }

  std::thread thread_;
};

class TestRocksDbRepository : public TestThreadedRepository {
 public:
  std::optional<RocksDbStats> getRocksDbStats() const override {
    return RocksDbStats {
      .table_readers_size = 100,
      .all_memory_tables_size = 200
    };
  }
};

class TestFlowRepository : public org::apache::nifi::minifi::core::ThreadedRepositoryImpl {
 public:
  TestFlowRepository()
    : org::apache::nifi::minifi::core::ThreadedRepositoryImpl("ff", "./dir", 1s, TEST_MAX_REPOSITORY_STORAGE_SIZE, 0ms) {
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &) override {
    return true;
  }

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.emplace(key, std::string{reinterpret_cast<const char*>(buf), bufLen});
    return true;
  }

  bool Delete(const std::string& key) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.erase(key);
    return true;
  }

  bool Get(const std::string &key, std::string &value) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    auto result = repository_results_.find(key);
    if (result != repository_results_.end()) {
      value = result->second;
      return true;
    } else {
      return false;
    }
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }

 private:
  void run() override {
  }

  std::thread& getThread() override {
    return thread_;
  }

 protected:
  mutable std::mutex repository_results_mutex_;
  std::map<std::string, std::string> repository_results_;
  std::shared_ptr<org::apache::nifi::minifi::core::ContentRepository> content_repo_;
  std::thread thread_;
};

class TestFlowController : public org::apache::nifi::minifi::FlowController {
 public:
  TestFlowController(std::shared_ptr<org::apache::nifi::minifi::core::Repository> repo, std::shared_ptr<org::apache::nifi::minifi::core::Repository> flow_file_repo,
      const std::shared_ptr<org::apache::nifi::minifi::core::ContentRepository>& /*content_repo*/)
      :org::apache::nifi::minifi::FlowController(repo, flow_file_repo, org::apache::nifi::minifi::Configure::create(), nullptr,
          std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>()) {
  }

  ~TestFlowController() override = default;

  void load(bool /*reload*/ = false) override {
  }

  int16_t start() override {
    return 0;
  }

  int16_t stop() override {
    return 0;
  }

  void waitUnload(const std::chrono::milliseconds /*time_to_wait*/) override {
    stop();
  }

  int16_t pause() override {
    return -1;
  }

  int16_t resume() override {
    return -1;
  }

  bool isRunning() const override {
    return true;
  }

  std::shared_ptr<org::apache::nifi::minifi::core::Processor> createProcessor(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  org::apache::nifi::minifi::core::ProcessGroup *createRootProcessGroup(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  org::apache::nifi::minifi::core::ProcessGroup *createRemoteProcessGroup(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  std::shared_ptr<org::apache::nifi::minifi::Connection> createConnection(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return nullptr;
  }
};
