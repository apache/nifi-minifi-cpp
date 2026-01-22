/**
 * @file SchedulingAgent.h
 * SchedulingAgent class declaration
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

#pragma once

#include <memory>
#include <string>
#include <set>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
#include "utils/CallBackTimer.h"
#include "utils/expected.h"
#include "utils/Monitors.h"
#include "utils/TimeUtil.h"
#include "utils/ThreadPool.h"
#include "utils/BackTrace.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "properties/Configure.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/controller/ControllerServiceProvider.h"

constexpr std::chrono::milliseconds SCHEDULING_WATCHDOG_CHECK_PERIOD = std::chrono::seconds(1);
constexpr std::chrono::milliseconds SCHEDULING_WATCHDOG_DEFAULT_ALERT_PERIOD = std::chrono::seconds(5);

namespace org::apache::nifi::minifi {

class SchedulingAgent {
 public:
  SchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_repo,
                  std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration, utils::ThreadPool& thread_pool)
      : admin_yield_duration_(),
        bored_yield_duration_(0),
        configure_(configuration),
        content_repo_(content_repo),
        thread_pool_(thread_pool),
        controller_service_provider_(controller_service_provider),
        logger_(core::logging::LoggerFactory<SchedulingAgent>::getLogger()) {
    running_ = false;
    repo_ = repo;
    flow_repo_ = flow_repo;

    alert_time_ = configuration->get(Configure::nifi_flow_engine_alert_period)
        | utils::andThen(utils::timeutils::StringToDuration<std::chrono::milliseconds>)
        | utils::valueOrElse([] { return SCHEDULING_WATCHDOG_DEFAULT_ALERT_PERIOD; });

    if (alert_time_ > std::chrono::milliseconds(0)) {
      std::function<void(void)> f = std::bind(&SchedulingAgent::watchDogFunc, this);
      watchDogTimer_.reset(new utils::CallBackTimer(SCHEDULING_WATCHDOG_CHECK_PERIOD, f));
      watchDogTimer_->start();
    }

    logger_->log_trace("Creating scheduling agent");
  }

  virtual ~SchedulingAgent() {
    // Do NOT remove this!
    // The destructor of the timer also stops is, but the stop should happen first!
    // Otherwise the callback might access already destructed members.
    watchDogTimer_.reset();
    logger_->log_trace("Destroying scheduling agent");
  }

  bool processorYields(core::Processor* processor) const;

  nonstd::expected<void, std::exception_ptr> triggerAndCommit(core::Processor* processor,
      const std::shared_ptr<core::ProcessContext>& process_context,
      const std::shared_ptr<core::ProcessSessionFactory>& session_factory);
  nonstd::expected<bool, std::exception_ptr> trigger(core::Processor* processor,
      const std::shared_ptr<core::ProcessContext>& process_context,
      const std::shared_ptr<core::ProcessSession>& process_session);

  void start() {
    running_ = true;
    thread_pool_.start();
  }

  virtual void stop() {
    running_ = false;
  }

  void watchDogFunc();

  // schedule, overwritten by different DrivenSchedulingAgent
  virtual void schedule(core::Processor* processor) = 0;
  // unschedule, overwritten by different DrivenSchedulingAgent
  virtual void unschedule(core::Processor* processor) = 0;

  SchedulingAgent(const SchedulingAgent &parent) = delete;
  SchedulingAgent &operator=(const SchedulingAgent &parent) = delete;

  std::chrono::milliseconds getAdminYieldDuration() const { return admin_yield_duration_; }

 protected:
  std::mutex mutex_;
  std::atomic<bool> running_;
  std::chrono::milliseconds admin_yield_duration_;
  std::chrono::milliseconds bored_yield_duration_;

  std::shared_ptr<Configure> configure_;

  std::shared_ptr<core::Repository> repo_;

  std::shared_ptr<core::Repository> flow_repo_;

  std::shared_ptr<core::ContentRepository> content_repo_;
  utils::ThreadPool& thread_pool_;
  gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider_;

 private:
  struct SchedulingInfo {
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    // Mutable is required to be able to modify this while leaving in std::set
    mutable std::chrono::steady_clock::time_point last_alert_time_ = std::chrono::steady_clock::now();
    std::string name_;
    std::string uuid_;

    explicit SchedulingInfo(const core::Processor* processor) :
      name_(processor->getName()),
      uuid_(processor->getUUIDStr()) {}

    bool operator <(const SchedulingInfo& o) const {
      return std::tie(start_time_, name_, uuid_) < std::tie(o.start_time_, o.name_, o.uuid_);
    }
  };

  std::shared_ptr<core::logging::Logger> logger_;
  mutable std::mutex watchdog_mtx_;  // used to protect the vector below
  std::vector<gsl::not_null<SchedulingInfo*>> scheduled_processors_;
  std::unique_ptr<utils::CallBackTimer> watchDogTimer_;
  std::chrono::milliseconds alert_time_;
};

}  // namespace org::apache::nifi::minifi
