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

namespace org::apache::nifi::minifi {

class Connection : public virtual core::Connectable {
 public:
  ~Connection() override = default;

  static constexpr uint64_t DEFAULT_BACKPRESSURE_THRESHOLD_COUNT = 2000;
  static constexpr uint64_t DEFAULT_BACKPRESSURE_THRESHOLD_DATA_SIZE = 100_MB;

  virtual void setSourceUUID(const utils::Identifier &uuid) = 0;
  virtual void setDestinationUUID(const utils::Identifier &uuid) = 0;
  virtual utils::Identifier getSourceUUID() const = 0;
  virtual utils::Identifier getDestinationUUID() const = 0;
  virtual void setSource(core::Connectable* source) = 0;
  virtual core::Connectable* getSource() const = 0;
  virtual void setDestination(core::Connectable* dest) = 0;
  virtual core::Connectable* getDestination() const = 0;
  virtual void addRelationship(core::Relationship relationship) = 0;
  virtual const std::set<core::Relationship> &getRelationships() const = 0;
  virtual void setBackpressureThresholdCount(uint64_t size) = 0;
  virtual uint64_t getBackpressureThresholdCount() const = 0;
  virtual void setBackpressureThresholdDataSize(uint64_t size) = 0;
  virtual uint64_t getBackpressureThresholdDataSize() const = 0;
  virtual void setSwapThreshold(uint64_t size) = 0;
  virtual void setFlowExpirationDuration(std::chrono::milliseconds duration) = 0;
  virtual std::chrono::milliseconds getFlowExpirationDuration() const = 0;
  virtual void setDropEmptyFlowFiles(bool drop) = 0;
  virtual bool getDropEmptyFlowFiles() const = 0;
  virtual bool isEmpty() const = 0;
  virtual bool backpressureThresholdReached() const = 0;
  virtual uint64_t getQueueSize() const = 0;
  virtual uint64_t getQueueDataSize() = 0;
  virtual void multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows) = 0;
  virtual std::shared_ptr<core::FlowFile> poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords) = 0;
  virtual void drain(bool delete_permanently) = 0;
};
}  // namespace org::apache::nifi::minifi
