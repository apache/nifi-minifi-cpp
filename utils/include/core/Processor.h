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

#include "core/ConfigurableComponent.h"
#include "core/Connectable.h"
#include "core/Core.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/Scheduling.h"
#include "utils/TimeUtil.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "minifi-cpp/core/ProcessorMetrics.h"
#include "utils/gsl.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "minifi-cpp/core/Processor.h"
#include "core/Property.h"
#include "core/ProcessContext.h"

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

class ProcessSession;
class ProcessSessionFactory;

constexpr std::chrono::microseconds MINIMUM_SCHEDULING_PERIOD{30};

#define BUILDING_DLL 1

class ProcessorImpl : public virtual Processor, public ConnectableImpl, public ConfigurableComponentImpl {
 public:
  ProcessorImpl(std::string_view name, const utils::Identifier& uuid, std::shared_ptr<ProcessorMetrics> metrics = nullptr);
  explicit ProcessorImpl(std::string_view name, std::shared_ptr<ProcessorMetrics> metrics = nullptr);

  ProcessorImpl(const ProcessorImpl& parent) = delete;
  ProcessorImpl& operator=(const ProcessorImpl& parent) = delete;

  bool isRunning() const override;

  ~ProcessorImpl() override;

  void setScheduledState(ScheduledState state) override;

  ScheduledState getScheduledState() const override {
    return state_;
  }

  void setSchedulingStrategy(SchedulingStrategy strategy) override {
    strategy_ = strategy;
  }

  SchedulingStrategy getSchedulingStrategy() const override {
    return strategy_;
  }

  void setSchedulingPeriod(std::chrono::steady_clock::duration period) override {
    scheduling_period_ = std::max(std::chrono::steady_clock::duration(MINIMUM_SCHEDULING_PERIOD), period);
  }

  std::chrono::steady_clock::duration getSchedulingPeriod() const override {
    return scheduling_period_;
  }

  void setCronPeriod(const std::string &period) override {
    cron_period_ = period;
  }

  std::string getCronPeriod() const override {
    return cron_period_;
  }

  void setRunDurationNano(std::chrono::steady_clock::duration period) override {
    run_duration_ = period;
  }

  std::chrono::steady_clock::duration getRunDurationNano() const override {
    return (run_duration_);
  }

  void setYieldPeriodMsec(std::chrono::milliseconds period) override {
    yield_period_ = period;
  }

  std::chrono::steady_clock::duration getYieldPeriod() const override {
    return yield_period_;
  }

  void setPenalizationPeriod(std::chrono::milliseconds period) override {
    penalization_period_ = period;
  }

  void setMaxConcurrentTasks(uint8_t tasks) override;

  bool isSingleThreaded() const override = 0;

  std::string getProcessorType() const override = 0;

  void setTriggerWhenEmpty(bool value) override {
    _triggerWhenEmpty = value;
  }

  bool getTriggerWhenEmpty() const override {
    return (_triggerWhenEmpty);
  }

  uint8_t getActiveTasks() const override {
    return (active_tasks_);
  }

  void incrementActiveTasks() override {
    ++active_tasks_;
  }

  void decrementActiveTask() override {
    if (active_tasks_ > 0)
      --active_tasks_;
  }

  void clearActiveTask() override {
    active_tasks_ = 0;
  }

  void yield() override;

  void yield(std::chrono::steady_clock::duration delta_time) override;

  bool isYield() override;

  void clearYield() override;

  std::chrono::steady_clock::time_point getYieldExpirationTime() const override { return yield_expiration_; }
  std::chrono::steady_clock::duration getYieldTime() const override;
  // Whether flow file queue full in any of the outgoing connection
  bool flowFilesOutGoingFull() const override;

  bool addConnection(Connectable* connection) override;

  bool canEdit() override {
    return !isRunning();
  }

  void initialize() override {
  }

  void triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory) override;
  void trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session) override;

  void onTrigger(ProcessContext&, ProcessSession&) override {}

  void onSchedule(ProcessContext&, ProcessSessionFactory&) override {}

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  void onUnSchedule() override {
    notifyStop();
  }

  // Check all incoming connections for work
  bool isWorkAvailable() override;

  bool isThrottledByBackpressure() const override;

  Connectable* pickIncomingConnection() override;

  void validateAnnotations() const override;

  annotation::Input getInputRequirement() const override = 0;

  state::response::SharedResponseNode getResponseNode() override {
    return metrics_;
  }

  gsl::not_null<std::shared_ptr<ProcessorMetrics>> getMetrics() const override {
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
  void updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force = false) override;

  const std::unordered_map<Connection*, std::unordered_set<Processor*>>& reachable_processors() const override {
    return reachable_processors_;
  }

  static bool partOfCycle(Connection* conn);

  // an outgoing connection allows us to reach these nodes
  std::unordered_map<Connection*, std::unordered_set<Processor*>> reachable_processors_;

  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
