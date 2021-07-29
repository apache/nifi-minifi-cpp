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
#include "core/Processor.h"
#include "Connection.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "provenance/Provenance.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

/**
 * Test repository
 */
class TestRepository : public core::Repository {
 public:
  TestRepository()
      : core::SerializableComponent("repo_name"),
        Repository("repo_name", "./dir", 1000, 100, 0) {
  }

  bool initialize(const std::shared_ptr<minifi::Configure> &) override {
    return true;
  }

  void start() override {
    running_ = true;
  }

  void stop() override {
    running_ = false;
  }

  void setFull() {
    repo_full_ = true;
  }

  ~TestRepository() override = default;

  bool isNoop() override {
    return false;
  }

  bool Put(std::string key, const uint8_t *buf, size_t bufLen) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.emplace(key, std::string{reinterpret_cast<const char*>(buf), bufLen});
    return true;
  }

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override {
    for (const auto& item : data) {
      if (!Put(item.first, item.second->getBuffer(), item.second->size())) {
        return false;
      }
    }
    return true;
  }

  bool Serialize(const std::string &key, const uint8_t *buffer, const size_t bufferSize) override {
    return Put(key, buffer, bufferSize);
  }

  bool Delete(std::string key) override {
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

  bool Serialize(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t /*max_size*/) override {
    return false;
  }

  bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    max_size = 0;
    for (const auto &entry : repository_results_) {
      if (max_size >= store.size()) {
        break;
      }
      std::shared_ptr<core::SerializableComponent> eventRead = store.at(max_size);
      eventRead->DeSerialize(reinterpret_cast<const uint8_t*>(entry.second.data()), entry.second.length());
      ++max_size;
    }
    return true;
  }

  bool Serialize(const std::shared_ptr<core::SerializableComponent>& /*store*/) override {
    return false;
  }

  bool DeSerialize(const std::shared_ptr<core::SerializableComponent> &store) override {
    std::string value;
    Get(store->getUUIDStr(), value);
    store->DeSerialize(reinterpret_cast<const uint8_t*>(value.c_str()), value.size());
    return true;
  }

  bool DeSerialize(const uint8_t* /*buffer*/, const size_t /*bufferSize*/) override {
    return false;
  }

  std::map<std::string, std::string> getRepoMap() const {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    return repository_results_;
  }

  void getProvenanceRecord(std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records, int maxSize) {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    for (const auto &entry : repository_results_) {
      if (records.size() >= static_cast<uint64_t>(maxSize))
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead = std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize(reinterpret_cast<const uint8_t*>(entry.second.data()), entry.second.length())) {
        records.push_back(eventRead);
      }
    }
  }

  void run() override {
    // do nothing
  }

 protected:
  mutable std::mutex repository_results_mutex_;
  std::map<std::string, std::string> repository_results_;
};

class TestFlowRepository : public core::Repository {
 public:
  TestFlowRepository()
      : core::SerializableComponent("ff"),
        core::Repository("ff", "./dir", 1000, 100, 0) {
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &) override {
    return true;
  }

  ~TestFlowRepository() override = default;

  bool Put(std::string key, const uint8_t *buf, size_t bufLen) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.emplace(key, std::string{reinterpret_cast<const char*>(buf), bufLen});
    return true;
  }
  // Delete
  bool Delete(std::string key) override {
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

  std::map<std::string, std::string> getRepoMap() const {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    return repository_results_;
  }

  void getProvenanceRecord(std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records, int maxSize) {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    for (const auto &entry : repository_results_) {
      if (records.size() >= static_cast<uint64_t>(maxSize))
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead = std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize(reinterpret_cast<const uint8_t*>(entry.second.data()), entry.second.length())) {
        records.push_back(eventRead);
      }
    }
  }

  void loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) override {
  }

  void run() override {
    // do nothing
  }

 protected:
  mutable std::mutex repository_results_mutex_;
  std::map<std::string, std::string> repository_results_;
};

class TestFlowController : public minifi::FlowController {
 public:
  TestFlowController(std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> /*content_repo*/)
      : minifi::FlowController(repo, flow_file_repo, std::make_shared<minifi::Configure>(), nullptr, std::make_shared<core::repository::VolatileContentRepository>(), "") {
  }

  ~TestFlowController() override = default;

  void load(const std::shared_ptr<core::ProcessGroup>& /*root*/ = nullptr, bool /*reload*/ = false) override {
  }

  int16_t start() override {
    running_.store(true);
    return 0;
  }

  int16_t stop() override {
    running_.store(false);
    return 0;
  }
  void waitUnload(const uint64_t /*timeToWaitMs*/) override {
    stop();
  }

  int16_t pause() override {
    return -1;
  }

  int16_t resume() override {
    return -1;
  }

  void unload() override {
    stop();
  }

  bool isRunning() override {
    return true;
  }

  std::shared_ptr<core::Processor> createProcessor(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return 0;
  }

  core::ProcessGroup *createRootProcessGroup(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return 0;
  }

  core::ProcessGroup *createRemoteProcessGroup(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return 0;
  }

  std::shared_ptr<minifi::Connection> createConnection(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return 0;
  }
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
