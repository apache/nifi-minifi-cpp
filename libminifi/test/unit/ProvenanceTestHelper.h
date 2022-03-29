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
#include "../StubShutdownAgent.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

using namespace std::literals::chrono_literals;

/**
 * Test repository
 */
class TestRepository : public org::apache::nifi::minifi::core::Repository {
 public:
  TestRepository()
      : org::apache::nifi::minifi::core::SerializableComponent("repo_name"),
        Repository("repo_name", "./dir", 1s, 100, 0ms) {
  }

  ~TestRepository() override {
    stop();
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &) override {
    return true;
  }

  void start() override {
    running_ = true;
  }

  void setFull() {
    repo_full_ = true;
  }

  bool isNoop() override {
    return false;
  }

  bool Put(std::string key, const uint8_t *buf, size_t bufLen) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    repository_results_.emplace(key, std::string{reinterpret_cast<const char*>(buf), bufLen});
    return true;
  }

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<org::apache::nifi::minifi::io::BufferStream>>>& data) override {
    for (const auto& item : data) {
      const auto buf = item.second->getBuffer().as_span<const uint8_t>();
      if (!Put(item.first, buf.data(), buf.size())) {
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

  bool Serialize(std::vector<std::shared_ptr<org::apache::nifi::minifi::core::SerializableComponent>>& /*store*/, size_t /*max_size*/) override {
    return false;
  }

  bool DeSerialize(std::vector<std::shared_ptr<org::apache::nifi::minifi::core::SerializableComponent>> &store, size_t &max_size) override {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    max_size = 0;
    for (const auto &entry : repository_results_) {
      if (max_size >= store.size()) {
        break;
      }
      const auto eventRead = store.at(max_size);
      eventRead->DeSerialize(gsl::make_span(entry.second).as_span<const std::byte>());
      ++max_size;
    }
    return true;
  }

  bool Serialize(const std::shared_ptr<org::apache::nifi::minifi::core::SerializableComponent>& /*store*/) override {
    return false;
  }

  bool DeSerialize(const std::shared_ptr<org::apache::nifi::minifi::core::SerializableComponent> &store) override {
    std::string value;
    Get(store->getUUIDStr(), value);
    store->DeSerialize(gsl::make_span(value).as_span<const std::byte>());
    return true;
  }

  bool DeSerialize(gsl::span<const std::byte>) override {
    return false;
  }

  std::map<std::string, std::string> getRepoMap() const {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    return repository_results_;
  }

  void getProvenanceRecord(std::vector<std::shared_ptr<org::apache::nifi::minifi::provenance::ProvenanceEventRecord>> &records, int maxSize) {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    for (const auto &entry : repository_results_) {
      if (records.size() >= static_cast<uint64_t>(maxSize))
        break;
      const auto eventRead = std::make_shared<org::apache::nifi::minifi::provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize(gsl::make_span(entry.second).as_span<const std::byte>())) {
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

class TestFlowRepository : public org::apache::nifi::minifi::core::Repository {
 public:
  TestFlowRepository()
      : org::apache::nifi::minifi::core::SerializableComponent("ff"),
        org::apache::nifi::minifi::core::Repository("ff", "./dir", 1s, 100, 0ms) {
  }

  ~TestFlowRepository() override {
    stop();
  }

  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &) override {
    return true;
  }

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

  void getProvenanceRecord(std::vector<std::shared_ptr<org::apache::nifi::minifi::provenance::ProvenanceEventRecord>> &records, int maxSize) {
    std::lock_guard<std::mutex> lock{repository_results_mutex_};
    for (const auto &entry : repository_results_) {
      if (records.size() >= static_cast<uint64_t>(maxSize))
        break;
      const auto eventRead = std::make_shared<org::apache::nifi::minifi::provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize(gsl::make_span(entry.second).as_span<const std::byte>())) {
        records.push_back(eventRead);
      }
    }
  }

  void loadComponent(const std::shared_ptr<org::apache::nifi::minifi::core::ContentRepository>& /*content_repo*/) override {
  }

  void run() override {
    // do nothing
  }

 protected:
  mutable std::mutex repository_results_mutex_;
  std::map<std::string, std::string> repository_results_;
};

class TestFlowController : public org::apache::nifi::minifi::FlowController {
 public:
  TestFlowController(std::shared_ptr<org::apache::nifi::minifi::core::Repository> repo, std::shared_ptr<org::apache::nifi::minifi::core::Repository> flow_file_repo,
      const std::shared_ptr<org::apache::nifi::minifi::core::ContentRepository>& /*content_repo*/)
      :org::apache::nifi::minifi::FlowController(repo, flow_file_repo, std::make_shared<org::apache::nifi::minifi::Configure>(), nullptr,
          std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>(), "",
          std::make_shared<org::apache::nifi::minifi::utils::file::FileSystem>(), std::make_unique<org::apache::nifi::minifi::test::StubShutdownAgent>()) {
  }

  ~TestFlowController() override = default;

  void load(std::unique_ptr<org::apache::nifi::minifi::core::ProcessGroup> /*root*/ = nullptr, bool /*reload*/ = false) override {
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

  std::shared_ptr<org::apache::nifi::minifi::core::Processor> createProcessor(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return 0;
  }

  org::apache::nifi::minifi::core::ProcessGroup *createRootProcessGroup(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return 0;
  }

  org::apache::nifi::minifi::core::ProcessGroup *createRemoteProcessGroup(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return 0;
  }

  std::shared_ptr<org::apache::nifi::minifi::Connection> createConnection(const std::string& /*name*/, const org::apache::nifi::minifi::utils::Identifier& /*uuid*/) {
    return 0;
  }
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
