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
#include "minifi-cpp/core/Annotation.h"
#include "DynamicProperty.h"
#include "Scheduling.h"
#include "utils/TimeUtil.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "ProcessorMetrics.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi {

class Connection;

namespace core {

class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;

class Processor : public virtual Connectable, public virtual ConfigurableComponent, public virtual state::response::ResponseNodeSource {
 public:
  ~Processor() override = default;

  virtual void setScheduledState(ScheduledState state) = 0;
  virtual ScheduledState getScheduledState() const = 0;
  virtual void setSchedulingStrategy(SchedulingStrategy strategy) = 0;
  virtual SchedulingStrategy getSchedulingStrategy() const = 0;
  virtual void setSchedulingPeriod(std::chrono::steady_clock::duration period) = 0;
  virtual std::chrono::steady_clock::duration getSchedulingPeriod() const = 0;
  virtual void setCronPeriod(const std::string &period) = 0;
  virtual std::string getCronPeriod() const = 0;
  virtual void setRunDurationNano(std::chrono::steady_clock::duration period) = 0;
  virtual std::chrono::steady_clock::duration getRunDurationNano() const = 0;
  virtual void setYieldPeriodMsec(std::chrono::milliseconds period) = 0;
  virtual std::chrono::steady_clock::duration getYieldPeriod() const = 0;
  virtual void setPenalizationPeriod(std::chrono::milliseconds period) = 0;
  virtual bool isSingleThreaded() const = 0;
  virtual std::string getProcessorType() const = 0;
  virtual void setTriggerWhenEmpty(bool value) = 0;
  virtual bool getTriggerWhenEmpty() const = 0;
  virtual uint8_t getActiveTasks() const = 0;
  virtual void incrementActiveTasks() = 0;
  virtual void decrementActiveTask() = 0;
  virtual void clearActiveTask() = 0;
  using Connectable::yield;
  virtual void yield(std::chrono::steady_clock::duration delta_time) = 0;
  virtual bool isYield() = 0;
  virtual void clearYield() = 0;
  virtual std::chrono::steady_clock::time_point getYieldExpirationTime() const = 0;
  virtual std::chrono::steady_clock::duration getYieldTime() const = 0;
  virtual bool flowFilesOutGoingFull() const = 0;
  virtual bool addConnection(Connectable* connection) = 0;
  virtual void triggerAndCommit(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSessionFactory>& session_factory) = 0;
  virtual void trigger(const std::shared_ptr<ProcessContext>& context, const std::shared_ptr<ProcessSession>& process_session) = 0;
  virtual void onTrigger(ProcessContext&, ProcessSession&) = 0;
  virtual void onSchedule(ProcessContext&, ProcessSessionFactory&) = 0;
  virtual void onUnSchedule() = 0;
  virtual bool isThrottledByBackpressure() const = 0;
  virtual void validateAnnotations() const = 0;
  virtual annotation::Input getInputRequirement() const = 0;
  virtual gsl::not_null<std::shared_ptr<ProcessorMetrics>> getMetrics() const = 0;

  virtual void updateReachability(const std::lock_guard<std::mutex>& graph_lock, bool force = false) = 0;
  virtual const std::unordered_map<Connection*, std::unordered_set<Processor*>>& reachable_processors() const = 0;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi
