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

#include "core/ConfigurableComponentImpl.h"
#include "core/Connectable.h"
#include "core/Property.h"
#include "core/Core.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/DynamicProperty.h"
#include "minifi-cpp/core/Scheduling.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "minifi-cpp/core/ProcessorMetrics.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "Processor.h"
#include "minifi-cpp/core/Processor.h"

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

constexpr std::chrono::microseconds MINIMUM_SCHEDULING_PERIOD{30};

#define BUILDING_DLL 1

class ProcessorProxy : public virtual Processor, public ConnectableImpl, public ConfigurableComponentImpl {
 public:
  ProcessorProxy(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<ProcessorApi> impl);
  explicit ProcessorProxy(std::string_view name, std::unique_ptr<ProcessorApi> impl);

  ProcessorProxy(const ProcessorProxy& parent) = delete;
  ProcessorProxy& operator=(const ProcessorProxy& parent) = delete;

  bool isRunning() const override;

  ~ProcessorProxy() override;

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

  bool isSingleThreaded() const override {
    return impl_->isSingleThreaded();
  }

  std::string getProcessorType() const override {
    return impl_->getProcessorType();
  }

  bool getTriggerWhenEmpty() const override {
    return impl_->getTriggerWhenEmpty();
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

  std::string getProcessGroupUUIDStr() const override {
    return process_group_uuid_;
  }

  void setProcessGroupUUIDStr(const std::string &uuid) override {
    process_group_uuid_ = uuid;
  }

  void yield() override;

  void yield(std::chrono::steady_clock::duration delta_time) override;

  bool isYield() override;

  void clearYield() override;

  std::chrono::steady_clock::time_point getYieldExpirationTime() const override { return yield_expiration_; }
  std::chrono::steady_clock::duration getYieldTime() const override;

  bool addConnection(Connectable* connection) override;

  bool canEdit() override {
    return !isRunning();
  }

  void initialize() override;

  void triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory) override;
  void trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session) override;

  void onTrigger(ProcessContext& context, ProcessSession& session) override {
    impl_->onTrigger(context, session);
  }

  void onSchedule(ProcessContext& context, ProcessSessionFactory& session_factory) override {
    impl_->onSchedule(context, session_factory);
  }

  // Hook executed when onSchedule fails (throws). Configuration should be reset in this
  void onUnSchedule() override {
    impl_->onUnSchedule();
  }

  // Check all incoming connections for work
  bool isWorkAvailable() override;

  bool isThrottledByBackpressure() const override;

  Connectable* pickIncomingConnection() override;

  void validateAnnotations() const override;

  annotation::Input getInputRequirement() const override {
    return impl_->getInputRequirement();
  }

  [[nodiscard]] bool supportsDynamicProperties() const override {
    return impl_->supportsDynamicProperties();
  }

  [[nodiscard]] bool supportsDynamicRelationships() const override {
    return impl_->supportsDynamicRelationships();
  }

  state::response::SharedResponseNode getResponseNode() override {
    return getMetrics();
  }

  gsl::not_null<std::shared_ptr<ProcessorMetrics>> getMetrics() const override {
    return impl_->getMetrics();
  }

  ProcessorApi& getImpl() const override {
    gsl_Assert(impl_);
    return *impl_;
  }

  void restore(const std::shared_ptr<FlowFile>& file) override {
    impl_->restore(file);
  }

  static constexpr auto DynamicProperties = std::array<DynamicProperty, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

 protected:
  std::atomic<ScheduledState> state_;

  std::atomic<std::chrono::steady_clock::duration> scheduling_period_;
  std::atomic<std::chrono::steady_clock::duration> run_duration_;
  std::atomic<std::chrono::steady_clock::duration> yield_period_;

  std::atomic<uint8_t> active_tasks_;

  std::string cron_period_;

  std::shared_ptr<logging::Logger> logger_;

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

  std::string process_group_uuid_;

 protected:
  std::unique_ptr<ProcessorApi> impl_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
