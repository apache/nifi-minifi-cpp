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
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>

#include "Core.h"
#include <condition_variable>
#include "core/logging/Logger.h"
#include "Relationship.h"
#include "RelationshipDefinition.h"
#include "Scheduling.h"
#include "core/state/FlowIdentifier.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

class FlowFile;

/**
 * Represents the base connectable component
 * Purpose: As in NiFi, this represents a connection point and allows the derived
 * object to be connected to other connectables.
 */
class Connectable : public CoreComponent {
 public:
  explicit Connectable(std::string name);

  explicit Connectable(std::string name, const utils::Identifier &uuid);

  Connectable(const Connectable &other) = delete;
  Connectable(Connectable &&other) = delete;

  Connectable& operator=(const Connectable &other) = delete;
  Connectable& operator=(Connectable&& other) = delete;

  void setSupportedRelationships(std::span<const core::RelationshipDefinition> relationships);

  bool isSupportedRelationship(const Relationship &relationship);

  std::vector<Relationship> getSupportedRelationships() const;

  void setAutoTerminatedRelationships(std::span<const core::Relationship> relationships);

  bool isAutoTerminated(const Relationship &relationship);

  std::chrono::milliseconds getPenalizationPeriod() const {
    return penalization_period_;
  }

  /**
   * Get outgoing connection based on relationship
   * @return set of outgoing connections.
   */
  virtual std::set<Connectable*> getOutGoingConnections(const std::string &relationship);

  virtual void put(const std::shared_ptr<FlowFile>& /*flow*/) {
  }

  virtual void restore(const std::shared_ptr<FlowFile>& file) {
    put(file);
  }

  /**
   * Gets and sets next incoming connection
   * @return next incoming connection
   */
  Connectable* getNextIncomingConnection();

  virtual Connectable* pickIncomingConnection();

  /**
   * @return true if incoming connections > 0
   */
  bool hasIncomingConnections() const {
    return !incoming_connections_.empty();
  }

  uint8_t getMaxConcurrentTasks() const {
    return max_concurrent_tasks_;
  }

  virtual void setMaxConcurrentTasks(uint8_t tasks) {
    max_concurrent_tasks_ = tasks;
  }
  /**
   * Yield
   */
  virtual void yield() = 0;

  ~Connectable() override;

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() const = 0;

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(std::chrono::milliseconds timeout);
  /**
   * Notify this processor that work may be available
   */

  void notifyWork();

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() = 0;

  /**
   * Sets the flow version for this connectable.
   */
  void setFlowIdentifier(const std::shared_ptr<state::FlowIdentifier> &version) {
    connectable_version_ = version;
  }

  /**
   * Returns theflow version
   * @returns flow version. can be null if a flow version is not tracked.
   */
  virtual std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const {
    return connectable_version_;
  }

 protected:
  // must hold the relationship_mutex_ before calling this
  Connectable* getNextIncomingConnectionImpl(const std::lock_guard<std::mutex>& relationship_mutex_lock);
  // Penalization Period in MilliSecond
  std::atomic<std::chrono::milliseconds> penalization_period_;

  uint8_t max_concurrent_tasks_;

  // Supported relationships
  std::map<std::string, core::Relationship> relationships_;
  // Autoterminated relationships
  std::map<std::string, core::Relationship> auto_terminated_relationships_;

  // Incoming connections
  std::set<Connectable*> incoming_connections_;
  // Incoming connection Iterator
  decltype(incoming_connections_)::iterator incoming_connections_Iter;
  // Outgoing connections map based on Relationship name
  std::map<std::string, std::set<Connectable*>> outgoing_connections_;

  // Mutex for protection
  mutable std::mutex relationship_mutex_;

  ///// work conditionals and locking mechanisms

  // Concurrent condition mutex for whether there is incoming work to do
  std::mutex work_available_mutex_;
  // Condition for whether there is incoming work to do
  std::atomic<bool> has_work_;
  // Scheduling Strategy
  std::atomic<SchedulingStrategy> strategy_;
  // Concurrent condition variable for whether there is incoming work to do
  std::condition_variable work_condition_;
  // version under which this connectable was created.
  std::shared_ptr<state::FlowIdentifier> connectable_version_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core
