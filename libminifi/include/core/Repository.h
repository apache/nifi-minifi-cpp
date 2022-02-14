/**
 * @file Repository
 * Repository class declaration
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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_H_

#include <memory>
#include <utility>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "core/ContentRepository.h"
#include "core/SerializableComponent.h"
#include "properties/Configure.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Property.h"
#include "ResourceClaim.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "Core.h"
#include "core/Connectable.h"
#include "core/TraceableResource.h"
#include "utils/BackTrace.h"

#ifndef WIN32
#include <sys/stat.h>
#endif

namespace org::apache::nifi::minifi::core {

#define REPOSITORY_DIRECTORY "./repo"
#define MAX_REPOSITORY_STORAGE_SIZE (10*1024*1024)  // 10M
constexpr auto MAX_REPOSITORY_ENTRY_LIFE_TIME = std::chrono::minutes(10);
constexpr auto REPOSITORY_PURGE_PERIOD = std::chrono::milliseconds(2500);

class Repository : public virtual core::SerializableComponent, public core::TraceableResource {
 public:
  /*
   * Constructor for the repository
   */
  explicit Repository(std::string repo_name = "Repository",
             std::string directory = REPOSITORY_DIRECTORY,
             std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
             int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
             std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        thread_(),
        repo_size_(0),
        logger_(logging::LoggerFactory<Repository>::getLogger()) {
    directory_ = directory;
    max_partition_millis_ = maxPartitionMillis;
    max_partition_bytes_ = maxPartitionBytes;
    purge_period_ = purgePeriod;
    running_ = false;
    repo_full_ = false;
  }

  // Destructor
  ~Repository() override {
    stop();
  }

  virtual bool isNoop() {
    return true;
  }

  virtual void flush();

  // initialize
  virtual bool initialize(const std::shared_ptr<Configure>& /*configure*/) {
    return true;
  }
  // Put
  virtual bool Put(std::string /*key*/, const uint8_t* /*buf*/, size_t /*bufLen*/) {
    return true;
  }

  virtual bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>& /*data*/) {
    return true;
  }

  // Delete
  virtual bool Delete(std::string /*key*/) {
    return true;
  }

  virtual bool Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues) {
    bool found = true;
    for (const auto& storedValue : storedValues) {
      found &= Delete(storedValue->getName());
    }
    return found;
  }

  void setConnectionMap(std::map<std::string, std::shared_ptr<core::Connectable>> &connectionMap) {
    this->connectionMap = connectionMap;
  }

  void setContainers(std::map<std::string, std::shared_ptr<core::Connectable>> &containers) {
    this->containers = containers;
  }

  virtual bool Get(const std::string& /*key*/, std::string& /*value*/) {
    return false;
  }

  // Run function for the thread
  virtual void run() {
    // no op
  }

  /**
   * Since SerializableComponents represent a runnable object, we should return traces
   */
  BackTrace getTraces() override {
    return TraceResolver::getResolver().getBackTrace(getName(), thread_.native_handle());
  }

  // Start the repository monitor thread
  virtual void start();
  // Stop the repository monitor thread
  virtual void stop();
  // whether the repo is full
  virtual bool isFull() {
    return repo_full_;
  }
  // whether the repo is enable
  bool isRunning() override {
    return running_;
  }

  /**
   * Specialization that allows us to serialize max_size objects into store.
   * the lambdaConstructor will create objects to put into store
   * @param store vector in which we can store serialized object
   * @param max_size reference that stores the max number of objects to retrieve and serialize.
   * upon return max_size will represent the number of serialized objects.
   * @return status of this operation
   *
   * Base implementation returns true;
   */
  virtual bool Serialize(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t /*max_size*/) {
    return true;
  }

  /**
   * Specialization that allows us to deserialize max_size objects into store.
   * @param store vector in which we can store deserialized object
   * @param max_size reference that stores the max number of objects to retrieve and deserialize.
   * upon return max_size will represent the number of deserialized objects.
   * @return status of this operation
   *
   * Base implementation returns true;
   */
  virtual bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t& /*max_size*/) {
    return true;
  }

  /**
   * Specialization that allows us to deserialize max_size objects into store.
   * the lambdaConstructor will create objects to put into store
   * @param store vector in which we can store deserialized object
   * @param max_size reference that stores the max number of objects to retrieve and deserialize.
   * upon return max_size will represent the number of deserialized objects.
   * @param lambdaConstructor reference that will create the objects for store
   * @return status of this operation
   *
   * Base implementation returns true;
   */
  virtual bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t& /*max_size*/, std::function<std::shared_ptr<core::SerializableComponent>()> /*lambdaConstructor*/) { // NOLINT
    return true;
  }

  /**
   * Base implementation returns true;
   */
  bool Serialize(const std::shared_ptr<core::SerializableComponent>& /*store*/) override {
    return true;
  }

  /**
   * Base implementation returns true;
   */
  bool DeSerialize(const std::shared_ptr<core::SerializableComponent>& /*store*/) override {
    return true;
  }

  /**
   * Base implementation returns true;
   */
  bool DeSerialize(gsl::span<const std::byte>) override {
    return true;
  }

  bool Serialize(const std::string &key, const uint8_t *buffer, const size_t bufferSize) override {
    return Put(key, buffer, bufferSize);
  }

  uint64_t incrementSize(const char* /*fpath*/, const struct stat *sb, int /*typeflag*/) {
    return (repo_size_ += sb->st_size);
  }

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) {
  }

  virtual uint64_t getRepoSize();

  std::string getDirectory() const {
    return directory_;
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Repository(const Repository &parent) = delete;
  Repository &operator=(const Repository &parent) = delete;

 protected:
  std::map<std::string, std::shared_ptr<core::Connectable>> containers;

  std::map<std::string, std::shared_ptr<core::Connectable>> connectionMap;
  // Mutex for protection
  std::mutex mutex_;
  // repository directory
  std::string directory_;
  // max db entry life time
  std::chrono::milliseconds max_partition_millis_;
  // max db size
  int64_t max_partition_bytes_;
  // purge period
  std::chrono::milliseconds purge_period_;
  // thread
  std::thread thread_;
  // whether the monitoring thread is running for the repo while it was enabled
  std::atomic<bool> running_;
  // whether stop accepting provenace event
  std::atomic<bool> repo_full_;
  // repoSize

  // size of the directory
  std::atomic<uint64_t> repo_size_;
  // Run function for the thread
  void threadExecutor() {
    run();
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core
#endif  // LIBMINIFI_INCLUDE_CORE_REPOSITORY_H_
