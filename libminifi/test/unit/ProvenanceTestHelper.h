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

#include "provenance/Provenance.h"
#include "FlowController.h"
#include "core/Repository.h"
#include "core/repository/FlowFileRepository.h"
#include "core/Core.h"
/**
 * Test repository
 */
class TestRepository : public core::Repository {
 public:
  TestRepository()
      : Repository("repo_name", "./dir", 1000, 100, 0) {
  }
  // initialize
  bool initialize() {
    return true;
  }

  // Destructor
  virtual ~TestRepository() {

  }

  bool Put(std::string key, uint8_t *buf, int bufLen) {
    repositoryResults.insert(
        std::pair<std::string, std::string>(
            key, std::string((const char*) buf, bufLen)));
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

  void getProvenanceRecord(
      std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records,
      int maxSize) {
    for (auto entry : repositoryResults) {
      if (records.size() >= maxSize)
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead =
          std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize((uint8_t*) entry.second.data(),
          entry.second.length())) {
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

class TestFlowRepository : public core::repository::FlowFileRepository {
 public:
  TestFlowRepository()
      : core::repository::FlowFileRepository("./", 1000, 100, 0) {
  }
  // initialize
  bool initialize() {
    return true;
  }

  // Destructor
  virtual ~TestFlowRepository() {

  }

  bool Put(std::string key, uint8_t *buf, int bufLen) {
    repositoryResults.insert(
        std::pair<std::string, std::string>(
            key, std::string((const char*) buf, bufLen)));
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

  void getProvenanceRecord(
      std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records,
      int maxSize) {
    for (auto entry : repositoryResults) {
      if (records.size() >= maxSize)
        break;
      std::shared_ptr<provenance::ProvenanceEventRecord> eventRead =
          std::make_shared<provenance::ProvenanceEventRecord>();

      if (eventRead->DeSerialize((uint8_t*) entry.second.data(),
          entry.second.length())) {
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



class TestFlowController : public minifi::FlowController {

public:
  TestFlowController(std::shared_ptr<core::Repository> repo,
                     std::shared_ptr<core::Repository> flow_file_repo)
      : minifi::FlowController(repo, flow_file_repo, std::make_shared<minifi::Configure>(), nullptr, "",true) {
  }
  ~TestFlowController() {

  }
  void load() {

  }

  bool start() {
    running_.store(true);
    return true;
  }

  void stop(bool force) {
    running_.store(false);
  }
  void waitUnload(const uint64_t timeToWaitMs) {
    stop(true);
  }

  void unload() {
    stop(true);
  }

  void reload(std::string file) {

  }

  bool isRunning() {
    return true;
  }

  std::shared_ptr<core::Processor> createProcessor(std::string name,
                                                   uuid_t uuid) {
    return 0;
  }

  core::ProcessGroup *createRootProcessGroup(std::string name, uuid_t uuid) {
    return 0;
  }

  core::ProcessGroup *createRemoteProcessGroup(std::string name, uuid_t uuid) {
    return 0;
  }

  std::shared_ptr<minifi::Connection> createConnection(std::string name,
      uuid_t uuid) {
    return 0;
  }
protected:
  void initializePaths(const std::string &adjustedFilename) {
  }
};

#endif /* LIBMINIFI_TEST_UNIT_PROVENANCETESTHELPER_H_ */
