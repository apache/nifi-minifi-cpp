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
#include "core/Core.h"
#include "core/Connectable.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "core/FlowFile.h"
#include "core/Repository.h"
#include "utils/FlowFileQueue.h"

namespace org::apache::nifi::minifi {

class Connection : public core::Connectable {
 public:
  explicit Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name);
  explicit Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid);
  explicit Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid,
                      const utils::Identifier &srcUUID);
  explicit Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid,
                      const utils::Identifier &srcUUID, const utils::Identifier &destUUID);
  // Destructor
  ~Connection() override = default;

  // Set Source Processor UUID
  void setSourceUUID(const utils::Identifier &uuid) {
    src_uuid_ = uuid;
  }
  // Set Destination Processor UUID
  void setDestinationUUID(const utils::Identifier &uuid) {
    dest_uuid_ = uuid;
  }
  // Get Source Processor UUID
  utils::Identifier getSourceUUID() const {
    return src_uuid_;
  }
  // Get Destination Processor UUID
  utils::Identifier getDestinationUUID() const {
    return dest_uuid_;
  }

  // Set Connection Source Processor
  void setSource(core::Connectable* source) {
    source_connectable_ = source;
  }
  // ! Get Connection Source Processor
  core::Connectable* getSource() const {
    return source_connectable_;
  }
  // Set Connection Destination Processor
  void setDestination(core::Connectable* dest) {
    dest_connectable_ = dest;
  }
  // ! Get Connection Destination Processor
  core::Connectable* getDestination() {
    return dest_connectable_;
  }

  /**
   * Deprecated function
   * Please use addRelationship.
   */
  void setRelationship(core::Relationship relationship) {
    relationships_.insert(relationship);
  }

  // Set Connection relationship
  void addRelationship(core::Relationship relationship) {
    relationships_.insert(relationship);
  }
  // ! Get Connection relationship
  const std::set<core::Relationship> &getRelationships() const {
    return relationships_;
  }
  // Set Max Queue Size
  void setMaxQueueSize(uint64_t size) {
    max_queue_size_ = size;
  }
  // Get Max Queue Size
  uint64_t getMaxQueueSize() {
    return max_queue_size_;
  }
  // Set Max Queue Data Size
  void setMaxQueueDataSize(uint64_t size) {
    max_data_queue_size_ = size;
  }
  // Get Max Queue Data Size
  uint64_t getMaxQueueDataSize() {
    return max_data_queue_size_;
  }
  // Set Flow expiration duration in millisecond
  void setFlowExpirationDuration(std::chrono::milliseconds duration) {
    expired_duration_ = duration;
  }
  // Get Flow expiration duration in millisecond
  std::chrono::milliseconds getFlowExpirationDuration() {
    return expired_duration_;
  }

  void setDropEmptyFlowFiles(bool drop) {
    drop_empty_ = drop;
  }

  bool getDropEmptyFlowFiles() const {
    return drop_empty_;
  }

  // Check whether the queue is empty
  bool isEmpty() const;
  // Check whether the queue is full to apply back pressure
  bool isFull() const;
  // Get queue size
  uint64_t getQueueSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }
  // Get queue data size
  uint64_t getQueueDataSize() {
    return queued_data_size_;
  }

  // Put the flow file into queue
  void put(const std::shared_ptr<core::FlowFile>& flow) override;

  // Put multiple flowfiles into the queue
  void multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows);
  // Poll the flow file from queue, the expired flow file record also being returned
  std::shared_ptr<core::FlowFile> poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords);
  // Drain the flow records
  void drain(bool delete_permanently);

  void yield() override {}

  bool isWorkAvailable() override {
    const std::lock_guard<std::mutex> lock{mutex_};
    return queue_.isWorkAvailable();
  }

  bool isRunning() override {
    return true;
  }

 protected:
  // Source Processor UUID
  utils::Identifier src_uuid_;
  // Destination Processor UUID
  utils::Identifier dest_uuid_;
  // Relationship for this connection
  std::set<core::Relationship> relationships_;
  // Source Processor (ProcessNode/Port)
  core::Connectable* source_connectable_ = nullptr;
  // Destination Processor (ProcessNode/Port)
  core::Connectable* dest_connectable_ = nullptr;
  // Max queue size to apply back pressure
  std::atomic<uint64_t> max_queue_size_ = 0;
  // Max queue data size to apply back pressure
  std::atomic<uint64_t> max_data_queue_size_ = 0;
  // Flow File Expiration Duration in= MilliSeconds
  std::atomic<std::chrono::milliseconds> expired_duration_ = std::chrono::milliseconds(0);
  // flow file repository
  std::shared_ptr<core::Repository> flow_repository_;
  // content repository reference.
  std::shared_ptr<core::ContentRepository> content_repo_;

 private:
  bool drop_empty_ = false;
  // Mutex for protection
  mutable std::mutex mutex_;
  // Queued data size
  std::atomic<uint64_t> queued_data_size_ = 0;
  // Queue for the Flow File
  utils::FlowFileQueue queue_;
  // flow repository
  // Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<Connection>::getLogger();
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Connection(const Connection &parent);
  Connection &operator=(const Connection &parent);
};
}  // namespace org::apache::nifi::minifi
