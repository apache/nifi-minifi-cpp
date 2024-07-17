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

#include <utils/Id.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ConfigurableComponent.h"
#include "Connectable.h"
#include "Core.h"
#include "core/Annotation.h"
#include "DynamicProperty.h"
#include "Scheduling.h"
#include "utils/TimeUtil.h"
#include "core/state/nodes/MetricsBase.h"
#include "ProcessorMetrics.h"
#include "utils/gsl.h"
#include "OutputAttributeDefinition.h"

#define ADD_GET_PROCESSOR_NAME \
  std::string getProcessorType() const override { \
    auto class_name = org::apache::nifi::minifi::core::className<decltype(*this)>(); \
    auto splitted = org::apache::nifi::minifi::utils::string::split(class_name, "::"); \
    return splitted[splitted.size() - 1]; \
  }

#define ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS \
  bool supportsDynamicProperties() const override { return SupportsDynamicProperties; } \
  bool supportsDynamicRelationships() const override { return SupportsDynamicRelationships; } \
  minifi::core::annotation::Input getInputRequirement() const override { return InputRequirement; } \
  bool isSingleThreaded() const override { return IsSingleThreaded; } \
  ADD_GET_PROCESSOR_NAME

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

constexpr std::chrono::microseconds MINIMUM_SCHEDULING_PERIOD{30};

#define BUILDING_DLL 1

class Processor : public Connectable, public ConfigurableComponent, public state::response::ResponseNodeSource {
 public:
  Processor(std::string_view name, const utils::Identifier& uuid, std::shared_ptr<ProcessorMetrics> metrics = nullptr);
  explicit Processor(std::string_view name, std::shared_ptr<ProcessorMetrics> metrics = nullptr);

  Processor(const Processor& parent) = delete;
  Processor& operator=(const Processor& parent) = delete;

  bool isRunning() const override;

  ~Processor() override;

  void setScheduledState(ScheduledState state);

  ScheduledState getScheduledState() const {
    return state_;
  }

  void setSchedulingStrategy(SchedulingStrategy strategy) {
    strategy_ = strategy;
  }

  SchedulingStrategy getSchedulingStrategy() const {
    return strategy_;
  }

  void setSchedulingPeriod(std::chrono::steady_clock::duration period) {
    scheduling_period_ = std::max(std::chrono::steady_clock::duration(MINIMUM_SCHEDULING_PERIOD), period);
  }

  std::chrono::steady_clock::duration getSchedulingPeriod() const {
    return scheduling_period_;
  }

  void setCronPeriod(const std::string &period) {
    cron_period_ = period;
  }

  std::string getCronPeriod() const {
    return cron_period_;
  }

  void setRunDurationNano(std::chrono::steady_clock::duration period) {
    run_duration_ = period;
  }

  std::chrono::steady_clock::duration getRunDurationNano() const {
    return (run_duration_);
  }

  void setYieldPeriodMsec(std::chrono::milliseconds period) {
    yield_period_ = period;
  }

  std::chrono::steady_clock::duration getYieldPeriod() const {
    return yield_period_;
  }

  void setPenalizationPeriod(std::chrono::milliseconds period) {
    penalization_period_ = period;
  }

  void setMaxConcurrentTasks(uint8_t tasks) override;

  virtual bool isSingleThreaded() const = 0;

  virtual std::string getProcessorType() const = 0;

  void setTriggerWhenEmpty(bool value) {
    _triggerWhenEmpty = value;
  }

  bool getTriggerWhenEmpty() const {
    return (_triggerWhenEmpty);
  }

  uint8_t getActiveTasks() const {
    return (active_tasks_);
  }

  void incrementActiveTasks() {
    ++active_tasks_;
  }

  void decrementActiveTask() {
    if (active_tasks_ > 0)
      --active_tasks_;
  }

  void clearActiveTask() {
    active_tasks_ = 0;
  }

  void yield() override;

  void yield(std::chrono::steady_clock::duration delta_time);

  virtual bool isYield();

  void clearYield();

  std::chrono::steady_clock::time_point getYieldExpirationTime() const { return yield_expiration_; }
  std::chrono::steady_clock::duration getYieldTime() const;
  // Whether flow file queue full in any of the outgoing connection
  bool flowFilesOutGoingFull() const;

  bool addConnection(Connectable* connection);

  bool canEdit() override {
    return !isRunning();
  }

  void initialize() override {
  }

  virtual void triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory);
  void trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session);

  virtual void onTrigger(ProcessContext&, ProcessSession&) {}

  virtual void onSchedule(ProcessContext&, ProcessSessionFactory&) {}

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  virtual void onUnSchedule() {
    notifyStop();
  }

  // Check all incoming connections for work
  bool isWorkAvailable() override;

  bool isThrottledByBackpressure() const;

  Connectable* pickIncomingConnection() override;

  void validateAnnotations() const;

  virtual annotation::Input getInputRequirement() const = 0;

  state::response::SharedResponseNode getResponseNode() override {
    return metrics_;
  }

  auto getMetrics() const {
    return metrics_;
  }

  static constexpr auto DynamicProperties = std::array<DynamicProperty, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

 protected:
  virtual void notifyStop() {
  }

  std::atomic<ScheduledState> state_;

  std::atomic<std::chrono::steady_clock::duration> scheduling_period_;
  std::atomic<std::chrono::steady_clock::duration> run_duration_;
  std::atomic<std::chrono::steady_clock::duration> yield_period_;

  std::atomic<uint8_t> active_tasks_;
  std::atomic<bool> _triggerWhenEmpty;

  std::string cron_period_;
  gsl::not_null<std::shared_ptr<ProcessorMetrics>> metrics_;

 private:
  mutable std::mutex mutex_;
  std::atomic<std::chrono::steady_clock::time_point> yield_expiration_{};

  static std::mutex& getGraphMutex() {
    static std::mutex mutex{};
    return mutex;
  }

  // must hold the graphMutex
  void updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force = false);

  static bool partOfCycle(Connection* conn);

  // an outgoing connection allows us to reach these nodes
  std::unordered_map<Connection*, std::unordered_set<Processor*>> reachable_processors_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
