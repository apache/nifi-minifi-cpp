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
#ifndef LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_
#define LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_

#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
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
  // initialize
  bool initialize() {
    return true;
  }

  void start() {
    running_ = true;
  }

  void stop() {
    running_ = false;
  }

  void setFull() {
    repo_full_ = true;
  }

  // Destructor
  virtual ~TestRepository() {

  }

  bool Put(std::string key, const uint8_t *buf, size_t bufLen) {
    repositoryResults.insert(std::pair<std::string, std::string>(key, std::string((const char*) buf, bufLen)));
    return true;
  }

  virtual bool Serialize(const std::string &key, const uint8_t *buffer, const size_t bufferSize) {
    return Put(key, buffer, bufferSize);
  }

  // Delete
  bool Delete(std::string key) {
    repositoryResults.erase(key);
    return true;
  }
  // Get
  bool Get(const std::string &key, std::string &value) {
    auto result = repositoryResults.find(key);
    if (result != repositoryResults.end()) {
      value = result->second;
      return true;
    } else {
      return false;
    }
  }

  virtual bool Serialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t max_size) {
    return false;
  }

  virtual bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) {
    max_size = 0;
    for (auto entry : repositoryResults) {
      std::shared_ptr<core::SerializableComponent> eventRead = store.at(max_size);

      if (eventRead->DeSerialize((uint8_t*) entry.second.data(), entry.second.length())) {
      }
      if (+max_size >= store.size()) {
        break;
      }
    }
    return true;
  }

  virtual bool Serialize(const std::shared_ptr<core::SerializableComponent> &store) {
    return false;
  }

  virtual bool DeSerialize(const std::shared_ptr<core::SerializableComponent> &store) {
    std::string value;
    Get(store->getUUIDStr(), value);
    store->DeSerialize(reinterpret_cast<uint8_t*>(const_cast<char*>(value.c_str())), value.size());
    return true;
  }

  virtual bool DeSerialize(const uint8_t *buffer, const size_t bufferSize) {
    return false;
  }

  const std::map<std::string, std::string> &getRepoMap() const {
    return repositoryResults;
  }

  void getProvenanceRecord(std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records, int maxSize) {
    for (auto entry : repositoryResults) {
      if (records.size() >= (uint64_t)maxSize)
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead = std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize((uint8_t*) entry.second.data(), entry.second.length())) {
        records.push_back(eventRead);
      }
    }
  }

  void run() {
    // do nothing
  }
 protected:
  std::map<std::string, std::string> repositoryResults;
};

class TestFlowRepository : public core::Repository {
 public:
  TestFlowRepository()
      : core::SerializableComponent("ff"),
        core::Repository("ff", "./dir", 1000, 100, 0) {
  }
  // initialize
  bool initialize() {
    return true;
  }

  // Destructor
  virtual ~TestFlowRepository() {

  }

  bool Put(std::string key, uint8_t *buf, int bufLen) {
    repositoryResults.insert(std::pair<std::string, std::string>(key, std::string((const char*) buf, bufLen)));
    return true;
  }
  // Delete
  bool Delete(std::string key) {
    repositoryResults.erase(key);
    return true;
  }
  // Get
  bool Get(std::string key, std::string &value) {
    auto result = repositoryResults.find(key);
    if (result != repositoryResults.end()) {
      value = result->second;
      return true;
    } else {
      return false;
    }
  }

  const std::map<std::string, std::string> &getRepoMap() const {
    return repositoryResults;
  }

  void getProvenanceRecord(std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records, int maxSize) {
    for (auto entry : repositoryResults) {
      if (records.size() >= (uint64_t)maxSize)
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead = std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize((uint8_t*) entry.second.data(), entry.second.length())) {
        records.push_back(eventRead);
      }
    }
  }

  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  }

  void run() {
    // do nothing
  }
 protected:
  std::map<std::string, std::string> repositoryResults;
};

class TestFlowController : public minifi::FlowController {

 public:
  TestFlowController(std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> content_repo)
      : minifi::FlowController(repo, flow_file_repo, std::make_shared<minifi::Configure>(), nullptr, std::make_shared<core::repository::VolatileContentRepository>(), "", true) {
  }
  ~TestFlowController() {

  }
  void load() {

  }

  int16_t start() {
    running_.store(true);
    return 0;
  }

  int16_t stop(bool force, uint64_t timeToWait = 0) {
    running_.store(false);
    return 0;
  }
  void waitUnload(const uint64_t timeToWaitMs) {
    stop(true);
  }

  int16_t pause() {
    return -1;
  }

  void unload() {
    stop(true);
  }

  void reload(std::string file) {

  }

  bool isRunning() {
    return true;
  }

  std::shared_ptr<core::Processor> createProcessor(std::string name, utils::Identifier &  uuid) {
    return 0;
  }

  core::ProcessGroup *createRootProcessGroup(std::string name, utils::Identifier &  uuid) {
    return 0;
  }

  core::ProcessGroup *createRemoteProcessGroup(std::string name, utils::Identifier &  uuid) {
    return 0;
  }

  std::shared_ptr<minifi::Connection> createConnection(std::string name, utils::Identifier &  uuid) {
    return 0;
  }
 protected:
  void initializePaths(const std::string &adjustedFilename) {
  }
};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
#endif /* LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_ */
