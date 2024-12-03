/**
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

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>

#include "Core.h"
#include <condition_variable>
#include "Relationship.h"
#include "Scheduling.h"
#include "minifi-cpp/core/state/FlowIdentifier.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

class FlowFile;

/**
 * Represents the base connectable component
 * Purpose: As in NiFi, this represents a connection point and allows the derived
 * object to be connected to other connectables.
 */
class Connectable : public virtual CoreComponent {
 public:
  virtual bool isSupportedRelationship(const Relationship &relationship) = 0;
  virtual std::vector<Relationship> getSupportedRelationships() const = 0;
  virtual void addAutoTerminatedRelationship(const core::Relationship& relationship) = 0;
  virtual void setAutoTerminatedRelationships(std::span<const core::Relationship> relationships) = 0;
  virtual bool isAutoTerminated(const Relationship &relationship) = 0;
  virtual std::chrono::milliseconds getPenalizationPeriod() const = 0;
  virtual std::set<Connectable*> getOutGoingConnections(const std::string &relationship) = 0;
  virtual void put(const std::shared_ptr<FlowFile>& /*flow*/) = 0;
  virtual void restore(const std::shared_ptr<FlowFile>& file) = 0;
  virtual Connectable* getNextIncomingConnection() = 0;
  virtual Connectable* pickIncomingConnection() = 0;
  virtual bool hasIncomingConnections() const = 0;
  virtual uint8_t getMaxConcurrentTasks() const = 0;
  virtual void setMaxConcurrentTasks(uint8_t tasks) = 0;
  virtual void yield() = 0;
  ~Connectable() override = default;
  virtual bool isRunning() const = 0;
  virtual void waitForWork(std::chrono::milliseconds timeout) = 0;
  virtual void notifyWork() = 0;
  virtual bool isWorkAvailable() = 0;
  virtual void setFlowIdentifier(const std::shared_ptr<state::FlowIdentifier> &version) = 0;
  virtual std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
