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

#ifndef LIBMINIFI_INCLUDE_CORE_CONNECTABLE_H_
#define LIBMINIFI_INCLUDE_CORE_CONNECTABLE_H_

#include <set>
#include "Core.h"
#include <condition_variable>
#include "core/logging/Logger.h"
#include "Relationship.h"
#include "Scheduling.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Represents the base connectable component
 * Purpose: As in NiFi, this represents a connection point and allows the derived
 * object to be connected to other connectables.
 */
class Connectable : public CoreComponent {
 public:

  explicit Connectable(std::string name, uuid_t uuid);

  explicit Connectable(const Connectable &&other);

  bool setSupportedRelationships(std::set<Relationship> relationships);

  // Whether the relationship is supported
  bool isSupportedRelationship(Relationship relationship);

  /**
   * Sets auto terminated relationships
   * @param relationships
   * @return result of set operation.
   */
  bool setAutoTerminatedRelationships(std::set<Relationship> relationships);

  // Check whether the relationship is auto terminated
  bool isAutoTerminated(Relationship relationship);

  // Get Processor penalization period in MilliSecond
  uint64_t getPenalizationPeriodMsec(void) {
    return (_penalizationPeriodMsec);
  }

  /**
   * Get outgoing connection based on relationship
   * @return set of outgoing connections.
   */
  std::set<std::shared_ptr<Connectable>> getOutGoingConnections(
      std::string relationship);

  /**
   * Get next incoming connection
   * @return next incoming connection
   */
  std::shared_ptr<Connectable> getNextIncomingConnection();

  /**
   * @return true if incoming connections > 0
   */
  bool hasIncomingConnections() {
    return (_incomingConnections.size() > 0);
  }

  uint8_t getMaxConcurrentTasks() {
    return max_concurrent_tasks_;
  }

  void setMaxConcurrentTasks(const uint8_t tasks) {
    max_concurrent_tasks_ = tasks;
  }
  /**
   * Yield
   */
  virtual void yield() = 0;

  virtual ~Connectable();

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() = 0;

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs);
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
   * Verify that this connectable can be stopped.
   * @return bool.
   */
  virtual bool verifyCanStop() {
    if (isRunning())
      return true;
    return false;
  }

 protected:

  // Penalization Period in MilliSecond
  std::atomic<uint64_t> _penalizationPeriodMsec;

  uint8_t max_concurrent_tasks_;

  // Supported relationships
  std::map<std::string, core::Relationship> relationships_;
  // Autoterminated relationships
  std::map<std::string, core::Relationship> auto_terminated_relationships_;

  // Incoming connection Iterator
  std::set<std::shared_ptr<Connectable>>::iterator incoming_connections_Iter;
  // Incoming connections
  std::set<std::shared_ptr<Connectable>> _incomingConnections;
  // Outgoing connections map based on Relationship name
  std::map<std::string, std::set<std::shared_ptr<Connectable>>> out_going_connections_;

  // Mutex for protection
  std::mutex relationship_mutex_;

  ///// work conditionals and locking mechanisms

  // Concurrent condition mutex for whether there is incoming work to do
  std::mutex work_available_mutex_;
  // Condition for whether there is incoming work to do
  std::atomic<bool> has_work_;
  // Scheduling Strategy
  std::atomic<SchedulingStrategy> strategy_;
  // Concurrent condition variable for whether there is incoming work to do
  std::condition_variable work_condition_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}
/* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONNECTABLE_H_ */
