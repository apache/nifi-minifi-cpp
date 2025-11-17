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
#include "minifi-cpp/core/Property.h"
#include "core/Core.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/DynamicPropertyDefinition.h"
#include "minifi-cpp/core/Scheduling.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/ProcessorMetrics.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Id.h"
#include "minifi-cpp/core/OutputAttributeDefinition.h"
#include "Processor.h"
#include "minifi-cpp/core/ProcessorApi.h"

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

class Processor : public ConnectableImpl, public ConfigurableComponentImpl, public state::response::ResponseNodeSource {
 public:
  Processor(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<ProcessorApi> impl);
  explicit Processor(std::string_view name, std::unique_ptr<ProcessorApi> impl);

  Processor(const Processor& parent) = delete;
  Processor& operator=(const Processor& parent) = delete;

  bool isRunning() const override;

  ~Processor() override;

  void setScheduledState(ScheduledState state);
  ScheduledState getScheduledState() const;
  void setSchedulingStrategy(SchedulingStrategy strategy);
  SchedulingStrategy getSchedulingStrategy() const;
  void setSchedulingPeriod(std::chrono::steady_clock::duration period);
  std::chrono::steady_clock::duration getSchedulingPeriod() const;
  void setCronPeriod(const std::string &period);
  std::string getCronPeriod() const;
  void setRunDurationNano(std::chrono::steady_clock::duration period);
  std::chrono::steady_clock::duration getRunDurationNano() const;
  void setYieldPeriodMsec(std::chrono::milliseconds period);
  std::chrono::steady_clock::duration getYieldPeriod() const;
  void setPenalizationPeriod(std::chrono::milliseconds period);
  void setMaxConcurrentTasks(uint8_t tasks) override;
  bool isSingleThreaded() const;
  std::string getProcessorType() const;
  bool getTriggerWhenEmpty() const;
  uint8_t getActiveTasks() const;
  void incrementActiveTasks();
  void decrementActiveTask();
  void clearActiveTask();
  std::string getProcessGroupUUIDStr() const;
  void setProcessGroupUUIDStr(const std::string &uuid);
  void yield() override;
  void yield(std::chrono::steady_clock::duration delta_time);
  bool isYield() const;
  void clearYield();
  std::chrono::steady_clock::time_point getYieldExpirationTime() const;
  std::chrono::steady_clock::duration getYieldTime() const;
  bool addConnection(Connectable* connection);
  bool canEdit() override;
  void initialize() override;
  void triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory);
  void trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session);
  void onTrigger(ProcessContext& context, ProcessSession& session);
  void onSchedule(ProcessContext& context, ProcessSessionFactory& session_factory);
  void onUnSchedule();
  bool isWorkAvailable() override;
  bool isThrottledByBackpressure() const;
  Connectable* pickIncomingConnection() override;
  void validateAnnotations() const;
  annotation::Input getInputRequirement() const;
  [[nodiscard]] bool supportsDynamicProperties() const override;
  [[nodiscard]] bool supportsDynamicRelationships() const override;
  state::response::SharedResponseNode getResponseNode() override;
  gsl::not_null<std::shared_ptr<ProcessorMetrics>> getMetrics() const;
  std::shared_ptr<ProcessorMetricsExtension> getMetricsExtension() const;
  std::string getProcessGroupName() const;
  void setProcessGroupName(const std::string &name);
  std::string getProcessGroupPath() const;
  void setProcessGroupPath(const std::string &path);
  logging::LOG_LEVEL getLogBulletinLevel() const;
  void setLogBulletinLevel(logging::LOG_LEVEL level);
  void setLoggerCallback(const std::function<void(logging::LOG_LEVEL level, const std::string& message)>& callback);
  void restore(const std::shared_ptr<FlowFile>& file) override;

  static constexpr auto DynamicProperties = std::array<DynamicPropertyDefinition, 0>{};

  static constexpr auto OutputAttributes = std::array<OutputAttributeReference, 0>{};

  ProcessorApi& getImpl() const {
    gsl_Assert(impl_);
    return *impl_;
  }

  template<typename T>
  T& getImpl() const {
    auto* res = dynamic_cast<T*>(&getImpl());
    gsl_Assert(res);
    return *res;
  }

 protected:
  std::atomic<ScheduledState> state_;

  std::atomic<std::chrono::steady_clock::duration> scheduling_period_;
  std::atomic<std::chrono::steady_clock::duration> run_duration_;
  std::atomic<std::chrono::steady_clock::duration> yield_period_;

  std::atomic<uint8_t> active_tasks_;

  std::string cron_period_;

  std::shared_ptr<logging::Logger> logger_;
  logging::LOG_LEVEL log_bulletin_level_ = logging::LOG_LEVEL::warn;

 private:
  mutable std::mutex mutex_;
  std::atomic<std::chrono::steady_clock::time_point> yield_expiration_{};

  // must hold the graphMutex
  void updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force = false);

  const std::unordered_map<Connection*, std::unordered_set<Processor*>>& reachable_processors() const;

  static bool partOfCycle(Connection* conn);

  // an outgoing connection allows us to reach these nodes
  std::unordered_map<Connection*, std::unordered_set<Processor*>> reachable_processors_;

  std::string process_group_uuid_;
  std::string process_group_name_;
  std::string process_group_path_;

  gsl::not_null<std::shared_ptr<ProcessorMetrics>> metrics_;

 protected:
  std::unique_ptr<ProcessorApi> impl_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
