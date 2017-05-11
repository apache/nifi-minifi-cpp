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
#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <set>
#include <chrono>
#include <functional>

#include "Connectable.h"
#include "ConfigurableComponent.h"
#include "io/StreamFactory.h"
#include "Property.h"
#include "utils/TimeUtil.h"
#include "Relationship.h"
#include "Connection.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessSessionFactory.h"
#include "Scheduling.h"

#include <stack>
#include "Site2SiteClientProtocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// Minimum scheduling period in Nano Second
#define MINIMUM_SCHEDULING_NANOS 30000

// Default yield period in second
#define DEFAULT_YIELD_PERIOD_SECONDS 1

// Default penalization period in second
#define DEFAULT_PENALIZATION_PERIOD_SECONDS 30

// Processor Class
class Processor : public Connectable, public ConfigurableComponent,
    public std::enable_shared_from_this<Processor> {

 public:
  // Constructor
  /*!
   * Create a new processor
   */
  Processor(std::string name, uuid_t uuid = NULL);
  // Destructor
  virtual ~Processor() {
  }

  bool isRunning();
  // Set Processor Scheduled State
  void setScheduledState(ScheduledState state);
  // Get Processor Scheduled State
  ScheduledState getScheduledState(void) {
    return state_;
  }
  // Set Processor Scheduling Strategy
  void setSchedulingStrategy(SchedulingStrategy strategy) {
    strategy_ = strategy;
  }
  // Get Processor Scheduling Strategy
  SchedulingStrategy getSchedulingStrategy(void) {
    return strategy_;
  }
  // Set Processor Loss Tolerant
  void setlossTolerant(bool lossTolerant) {
    loss_tolerant_ = lossTolerant;
  }
  // Get Processor Loss Tolerant
  bool getlossTolerant(void) {
    return loss_tolerant_;
  }
  // Set Processor Scheduling Period in Nano Second
  void setSchedulingPeriodNano(uint64_t period) {
    uint64_t minPeriod = MINIMUM_SCHEDULING_NANOS;
    scheduling_period_nano_ = std::max(period, minPeriod);
  }
  // Get Processor Scheduling Period in Nano Second
  uint64_t getSchedulingPeriodNano(void) {
    return scheduling_period_nano_;
  }
  // Set Processor Run Duration in Nano Second
  void setRunDurationNano(uint64_t period) {
    run_durantion_nano_ = period;
  }
  // Get Processor Run Duration in Nano Second
  uint64_t getRunDurationNano(void) {
    return (run_durantion_nano_);
  }
  // Set Processor yield period in MilliSecond
  void setYieldPeriodMsec(uint64_t period) {
    yield_period_msec_ = period;
  }
  // Get Processor yield period in MilliSecond
  uint64_t getYieldPeriodMsec(void) {
    return (yield_period_msec_);
  }
  // Set Processor penalization period in MilliSecond
  void setPenalizationPeriodMsec(uint64_t period) {
    _penalizationPeriodMsec = period;
  }

  // Set Processor Maximum Concurrent Tasks
  void setMaxConcurrentTasks(uint8_t tasks) {
    max_concurrent_tasks_ = tasks;
  }
  // Get Processor Maximum Concurrent Tasks
  uint8_t getMaxConcurrentTasks(void) {
    return (max_concurrent_tasks_);
  }
  // Set Trigger when empty
  void setTriggerWhenEmpty(bool value) {
    _triggerWhenEmpty = value;
  }
  // Get Trigger when empty
  bool getTriggerWhenEmpty(void) {
    return (_triggerWhenEmpty);
  }
  // Get Active Task Counts
  uint8_t getActiveTasks(void) {
    return (active_tasks_);
  }
  // Increment Active Task Counts
  void incrementActiveTasks(void) {
    active_tasks_++;
  }
  // decrement Active Task Counts
  void decrementActiveTask(void) {
    active_tasks_--;
  }
  void clearActiveTask(void) {
    active_tasks_ = 0;
  }
  // Yield based on the yield period
  void yield() {
    yield_expiration_ = (getTimeMillis() + yield_period_msec_);
  }
  // Yield based on the input time
  void yield(uint64_t time) {
    yield_expiration_ = (getTimeMillis() + time);
  }
  // whether need be to yield
  bool isYield() {
    if (yield_expiration_ > 0)
      return (yield_expiration_ >= getTimeMillis());
    else
      return false;
  }
  // clear yield expiration
  void clearYield() {
    yield_expiration_ = 0;
  }
  // get yield time
  uint64_t getYieldTime() {
    uint64_t curTime = getTimeMillis();
    if (yield_expiration_ > curTime)
      return (yield_expiration_ - curTime);
    else
      return 0;;
  }
  // Whether flow file queued in incoming connection
  bool flowFilesQueued();
  // Whether flow file queue full in any of the outgoin connection
  bool flowFilesOutGoingFull();

  // Get outgoing connections based on relationship name
  std::set<std::shared_ptr<Connection> > getOutGoingConnections(
      std::string relationship);
  // Add connection
  bool addConnection(std::shared_ptr<Connectable> connection);
  // Remove connection
  void removeConnection(std::shared_ptr<Connectable> connection);
  // Get the UUID as string
  std::string getUUIDStr() {
    return uuidStr_;
  }
  // Get the Next RoundRobin incoming connection
  std::shared_ptr<Connection> getNextIncomingConnection();
  // On Trigger
  void onTrigger(ProcessContext *context,
                 ProcessSessionFactory *sessionFactory);

  virtual bool canEdit() {
    return !isRunning();
  }

 public:

  // OnTrigger method, implemented by NiFi Processor Designer
  virtual void onTrigger(ProcessContext *context, ProcessSession *session) = 0;
  // Initialize, overridden by NiFi Process Designer
  virtual void initialize() {
  }
  // Scheduled event hook, overridden by NiFi Process Designer
  virtual void onSchedule(ProcessContext *context,
                          ProcessSessionFactory *sessionFactory) {
  }

 protected:

  // Processor state
  std::atomic<ScheduledState> state_;

  // lossTolerant
  std::atomic<bool> loss_tolerant_;
  // SchedulePeriod in Nano Seconds
  std::atomic<uint64_t> scheduling_period_nano_;
  // Run Duration in Nano Seconds
  std::atomic<uint64_t> run_durantion_nano_;
  // Yield Period in Milliseconds
  std::atomic<uint64_t> yield_period_msec_;

  // Active Tasks
  std::atomic<uint8_t> active_tasks_;
  // Trigger the Processor even if the incoming connection is empty
  std::atomic<bool> _triggerWhenEmpty;

  private:

  // Mutex for protection
  std::mutex mutex_;
  // Yield Expiration
  std::atomic<uint64_t> yield_expiration_;

  // Check all incoming connections for work
  bool isWorkAvailable();
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Processor(const Processor &parent);
  Processor &operator=(const Processor &parent);

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}
/* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
