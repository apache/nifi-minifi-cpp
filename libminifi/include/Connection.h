/**
 * @file Connection.h
 * Connection class declaration
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

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <utility>
#include "core/Core.h"
#include "core/Connectable.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "core/FlowFile.h"
#include "core/Repository.h"
#include "utils/FlowFileQueue.h"
#include "minifi-cpp/Connection.h"

namespace org::apache::nifi::minifi {

namespace test::utils {
struct ConnectionTestAccessor;
}  // namespace test::utils

class ConnectionImpl : public core::ConnectableImpl, public virtual Connection {
  friend struct test::utils::ConnectionTestAccessor;
 public:
  explicit ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name);
  explicit ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid);
  explicit ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid,
                      const utils::Identifier &srcUUID);
  explicit ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid,
                      const utils::Identifier &srcUUID, const utils::Identifier &destUUID);
  explicit ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<SwapManager> swap_manager,
                      std::string_view name, const utils::Identifier& uuid);
  ~ConnectionImpl() override = default;

  ConnectionImpl(const ConnectionImpl &parent) = delete;
  ConnectionImpl &operator=(const ConnectionImpl &parent) = delete;

  static constexpr uint64_t DEFAULT_BACKPRESSURE_THRESHOLD_COUNT = 2000;
  static constexpr uint64_t DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE = 100_MB;

  void setSourceUUID(const utils::Identifier &uuid) override {
    src_uuid_ = uuid;
  }

  void setDestinationUUID(const utils::Identifier &uuid) override {
    dest_uuid_ = uuid;
  }

  utils::Identifier getSourceUUID() const override {
    return src_uuid_;
  }

  utils::Identifier getDestinationUUID() const override {
    return dest_uuid_;
  }

  void setSource(core::Connectable* source) override {
    source_connectable_ = source;
  }

  core::Connectable* getSource() const override {
    return source_connectable_;
  }

  void setDestination(core::Connectable* dest) override {
    dest_connectable_ = dest;
  }

  core::Connectable* getDestination() const override {
    return dest_connectable_;
  }

  void addRelationship(core::Relationship relationship) override {
    relationships_.insert(std::move(relationship));
  }

  const std::set<core::Relationship> &getRelationships() const override {
    return relationships_;
  }

  void setBackpressureThresholdCount(uint64_t size) override {
    backpressure_threshold_count_ = size;
  }

  uint64_t getBackpressureThresholdCount() const override {
    return backpressure_threshold_count_;
  }

  void setBackpressureThresholdDataSize(uint64_t size) override {
    backpressure_threshold_data_size_ = size;
  }

  uint64_t getBackpressureThresholdDataSize() const override {
    return backpressure_threshold_data_size_;
  }

  void setSwapThreshold(uint64_t size) override {
    queue_.setTargetSize(size);
    queue_.setMinSize(size / 2);
    queue_.setMaxSize(size * 3 / 2);
  }

  void setFlowExpirationDuration(std::chrono::milliseconds duration) override {
    expired_duration_ = duration;
  }

  std::chrono::milliseconds getFlowExpirationDuration() const override {
    return expired_duration_;
  }

  void setDropEmptyFlowFiles(bool drop) override {
    drop_empty_ = drop;
  }

  bool getDropEmptyFlowFiles() const override {
    return drop_empty_;
  }

  bool isEmpty() const override;

  bool backpressureThresholdReached() const override;

  uint64_t getQueueSize() const override {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  uint64_t getQueueDataSize() override {
    return queued_data_size_;
  }

  void put(const std::shared_ptr<core::FlowFile>& flow) override;

  void multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows) override;

  std::shared_ptr<core::FlowFile> poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords) override;

  void drain(bool delete_permanently) override;

  void yield() override {}

  bool isWorkAvailable() override {
    const std::lock_guard<std::mutex> lock{mutex_};
    return queue_.isWorkAvailable();
  }

  bool isRunning() const override {
    return true;
  }

 protected:
  utils::Identifier src_uuid_;
  utils::Identifier dest_uuid_;
  std::set<core::Relationship> relationships_;
  core::Connectable* source_connectable_ = nullptr;
  core::Connectable* dest_connectable_ = nullptr;
  std::atomic<uint64_t> backpressure_threshold_count_ = DEFAULT_BACKPRESSURE_THRESHOLD_COUNT;
  std::atomic<uint64_t> backpressure_threshold_data_size_ = DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE;
  std::atomic<std::chrono::milliseconds> expired_duration_ = std::chrono::milliseconds(0);
  std::shared_ptr<core::Repository> flow_repository_;
  std::shared_ptr<core::ContentRepository> content_repo_;

 private:
  bool drop_empty_ = false;
  mutable std::mutex mutex_;
  std::atomic<uint64_t> queued_data_size_ = 0;
  utils::FlowFileQueue queue_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<Connection>::getLogger();
};
}  // namespace org::apache::nifi::minifi
