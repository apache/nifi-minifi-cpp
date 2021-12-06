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
#ifndef LIBMINIFI_INCLUDE_CORE_PROCESSOR_H_
#define LIBMINIFI_INCLUDE_CORE_PROCESSOR_H_

#include <utils/Id.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "ConfigurableComponent.h"
#include "Connectable.h"
#include "Connection.h"
#include "Core.h"
#include "core/Annotation.h"
#include "io/StreamFactory.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessSessionFactory.h"
#include "Property.h"
#include "Relationship.h"
#include "Scheduling.h"
#include "utils/TimeUtil.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// Minimum scheduling period in Nano Second
constexpr std::chrono::nanoseconds MINIMUM_SCHEDULING_NANOS{30000};

// Default penalization period in second

#define BUILDING_DLL 1
// Processor Class
class Processor : public Connectable, public ConfigurableComponent, public std::enable_shared_from_this<Processor> {
 public:
  Processor(const std::string& name, const utils::Identifier& uuid);
  explicit Processor(const std::string& name);
  virtual ~Processor() {
    notifyStop();
  }

  bool isRunning() override;
  // Set Processor Scheduled State
  void setScheduledState(ScheduledState state);
  // Get Processor Scheduled State
  ScheduledState getScheduledState() const {
    return state_;
  }
  // Set Processor Scheduling Strategy
  void setSchedulingStrategy(SchedulingStrategy strategy) {
    strategy_ = strategy;
  }
  // Get Processor Scheduling Strategy
  SchedulingStrategy getSchedulingStrategy() const {
    return strategy_;
  }
  // Set Processor Loss Tolerant
  void setlossTolerant(bool lossTolerant) {
    loss_tolerant_ = lossTolerant;
  }
  // Get Processor Loss Tolerant
  bool getlossTolerant() const {
    return loss_tolerant_;
  }
  // Set Processor Scheduling Period in Nano Second
  void setSchedulingPeriodNano(std::chrono::nanoseconds period) {
    scheduling_period_nano_ = std::max(MINIMUM_SCHEDULING_NANOS, period);
  }
  // Get Processor Scheduling Period in Nano Second
  std::chrono::nanoseconds getSchedulingPeriodNano() const {
    return scheduling_period_nano_;
  }

  /**
   * Sets the cron period
   * @param period cron period.
   */
  void setCronPeriod(const std::string &period) {
    cron_period_ = period;
  }

  /**
   * Returns the cron period
   * @return cron period
   */
  const std::string getCronPeriod() const {
    return cron_period_;
  }

  // Set Processor Run Duration in Nano Second
  void setRunDurationNano(std::chrono::nanoseconds period) {
    run_duration_nano_ = period;
  }
  // Get Processor Run Duration in Nano Second
  std::chrono::nanoseconds getRunDurationNano() const {
    return (run_duration_nano_);
  }
  // Set Processor yield period in MilliSecond
  void setYieldPeriodMsec(std::chrono::milliseconds period) {
    yield_period_msec_ = period;
  }
  // Get Processor yield period in MilliSecond
  std::chrono::milliseconds getYieldPeriodMsec() const {
    return yield_period_msec_;
  }

  void setPenalizationPeriod(std::chrono::milliseconds period) {
    penalization_period_ = period;
  }

  // Set Processor Maximum Concurrent Tasks
  void setMaxConcurrentTasks(uint8_t tasks) override;

  // Overriding to yield true can be used to indicate that the Processor is not safe for concurrent execution
  // of its onTrigger() method. By default, Processors are assumed to be safe for concurrent execution.
  virtual bool isSingleThreaded() const {
    return false;
  }

  // Set Trigger when empty
  void setTriggerWhenEmpty(bool value) {
    _triggerWhenEmpty = value;
  }
  // Get Trigger when empty
  bool getTriggerWhenEmpty() const {
    return (_triggerWhenEmpty);
  }
  // Get Active Task Counts
  uint8_t getActiveTasks() const {
    return (active_tasks_);
  }
  // Increment Active Task Counts
  void incrementActiveTasks() {
    active_tasks_++;
  }
  // decrement Active Task Counts
  void decrementActiveTask() {
    if (active_tasks_ > 0)
      active_tasks_--;
  }
  void clearActiveTask() {
    active_tasks_ = 0;
  }
  void yield() override;

  void yield(std::chrono::milliseconds delta_time);

  virtual bool isYield();

  void clearYield();

  std::chrono::milliseconds getYieldTime() const;
  // Whether flow file queue full in any of the outgoing connection
  bool flowFilesOutGoingFull() const;

  bool addConnection(std::shared_ptr<Connectable> connection);
  void removeConnection(std::shared_ptr<Connectable> connection);

  virtual void onTrigger(const std::shared_ptr<ProcessContext> &context, const std::shared_ptr<ProcessSessionFactory> &sessionFactory);
  void onTrigger(ProcessContext *context, ProcessSessionFactory *sessionFactory);

  bool canEdit() override {
    return !isRunning();
  }

 public:
  // OnTrigger method, implemented by NiFi Processor Designer
  virtual void onTrigger(const std::shared_ptr<ProcessContext> &context, const std::shared_ptr<ProcessSession> &session) {
    onTrigger(context.get(), session.get());
  }
  virtual void onTrigger(ProcessContext* /*context*/, ProcessSession* /*session*/) {
  }
  // Initialize, overridden by NiFi Process Designer
  void initialize() override {
  }
  // Scheduled event hook, overridden by NiFi Process Designer
  virtual void onSchedule(const std::shared_ptr<ProcessContext> &context, const std::shared_ptr<ProcessSessionFactory> &sessionFactory) {
    onSchedule(context.get(), sessionFactory.get());
  }
  virtual void onSchedule(ProcessContext* /*context*/, ProcessSessionFactory* /*sessionFactory*/) {
  }

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  virtual void onUnSchedule() {
    notifyStop();
  }

  // Check all incoming connections for work
  bool isWorkAvailable() override;

  void setStreamFactory(std::shared_ptr<minifi::io::StreamFactory> stream_factory) {
    stream_factory_ = stream_factory;
  }

  bool supportsDynamicProperties() override {
    return false;
  }

  bool isThrottledByBackpressure() const;

  std::shared_ptr<Connectable> pickIncomingConnection() override;

  void validateAnnotations() const;

  std::string getInputRequirementAsString() const;

 protected:
  virtual void notifyStop() {
  }

  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;

  // Processor state
  std::atomic<ScheduledState> state_;

  // lossTolerant
  std::atomic<bool> loss_tolerant_;
  // SchedulePeriod in Nano Seconds
  std::atomic<std::chrono::nanoseconds> scheduling_period_nano_;
  // Run Duration in Nano Seconds
  std::atomic<std::chrono::nanoseconds> run_duration_nano_;
  // Yield Period in Milliseconds
  std::atomic<std::chrono::milliseconds> yield_period_msec_;

  // Active Tasks
  std::atomic<uint8_t> active_tasks_;
  // Trigger the Processor even if the incoming connection is empty
  std::atomic<bool> _triggerWhenEmpty;

  std::string cron_period_;

 private:
  // Mutex for protection
  mutable std::mutex mutex_;
  // Yield Expiration
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> yield_expiration_;

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Processor(const Processor &parent);
  Processor &operator=(const Processor &parent);

 private:
  static std::mutex& getGraphMutex() {
    static std::mutex mutex{};
    return mutex;
  }

  // must hold the graphMutex
  void updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force = false);

  static bool partOfCycle(const std::shared_ptr<Connection>& conn);

  virtual annotation::Input getInputRequirement() const {
      // default input requirement
      return annotation::Input::INPUT_ALLOWED;
  }

  // an outgoing connection allows us to reach these nodes
  std::unordered_map<std::shared_ptr<Connection>, std::unordered_set<std::shared_ptr<const Processor>>> reachable_processors_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace core
/* namespace core */
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_PROCESSOR_H_
