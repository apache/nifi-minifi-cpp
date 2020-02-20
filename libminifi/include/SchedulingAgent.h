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
#ifndef __SCHEDULING_AGENT_H__
#define __SCHEDULING_AGENT_H__

#include <set>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
#include "utils/CallBackTimer.h"
#include "utils/Monitors.h"
#include "utils/TimeUtil.h"
#include "utils/ThreadPool.h"
#include "utils/BackTrace.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "FlowFileRecord.h"
#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceNode.h"

#define SCHEDULING_WATCHDOG_CHECK_PERIOD_MS 1000  // msec
#define SCHEDULING_WATCHDOG_DEFAULT_ALERT_PERIOD_MS 5000  // msec

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// SchedulingAgent Class
class SchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new scheduling agent.
   */
  SchedulingAgent(std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider, std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_repo,
                  std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration)
      : admin_yield_duration_(0),
        bored_yield_duration_(0),
        configure_(configuration),
        content_repo_(content_repo),
        controller_service_provider_(controller_service_provider),
        logger_(logging::LoggerFactory<SchedulingAgent>::getLogger()),
        alert_time_(configuration->getInt(Configure::nifi_flow_engine_alert_period, SCHEDULING_WATCHDOG_DEFAULT_ALERT_PERIOD_MS)) {
    running_ = false;
    repo_ = repo;
    flow_repo_ = flow_repo;
    /**
     * To facilitate traces we cannot use daemon threads -- this could potentially cause blocking on I/O; however, it's a better path
     * to be able to debug why an agent doesn't work and still allow a restart via updates in these cases.
     */
    auto csThreads = configure_->getInt(Configure::nifi_flow_engine_threads, 2);
    auto pool = utils::ThreadPool<utils::ComplexTaskResult>(csThreads, false, controller_service_provider, "SchedulingAgent");
    thread_pool_ = std::move(pool);
    thread_pool_.start();

    if (alert_time_ > std::chrono::milliseconds(0)) {
      std::function<void(void)> f = std::bind(&SchedulingAgent::watchDogFunc, this);
      watchDogTimer_.reset(new utils::CallBackTimer(std::chrono::milliseconds(SCHEDULING_WATCHDOG_CHECK_PERIOD_MS), f));
      watchDogTimer_->start();
    }
  }

  virtual ~SchedulingAgent() {
    // Do NOT remove this!
    // The destructor of the timer also stops is, but the stop should happen first!
    // Otherwise the callback might access already destructed members.
    watchDogTimer_.reset();
  }

  // onTrigger, return whether the yield is need
  bool onTrigger(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory);
  // Whether agent has work to do
  bool hasWorkToDo(std::shared_ptr<core::Processor> processor);
  // Whether the outgoing need to be backpressure
  bool hasTooMuchOutGoing(std::shared_ptr<core::Processor> processor);
  // start
  void start() {
    running_ = true;
    thread_pool_.start();
  }
  // stop
  virtual void stop() {
    running_ = false;
    thread_pool_.shutdown();
  }

  std::vector<BackTrace> getTraces() {
    return thread_pool_.getTraces();
  }

  void watchDogFunc();

  virtual std::future<utils::ComplexTaskResult> enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);
  virtual std::future<utils::ComplexTaskResult> disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);
  // schedule, overwritten by different DrivenSchedulingAgent
  virtual void schedule(std::shared_ptr<core::Processor> processor) = 0;
  // unschedule, overwritten by different DrivenSchedulingAgent
  virtual void unschedule(std::shared_ptr<core::Processor> processor) = 0;

  SchedulingAgent(const SchedulingAgent &parent) = delete;
  SchedulingAgent &operator=(const SchedulingAgent &parent) = delete;
 protected:
  // Mutex for protection
  std::mutex mutex_;
  // Whether it is running
  std::atomic<bool> running_;
  // AdministrativeYieldDuration
  int64_t admin_yield_duration_;
  // BoredYieldDuration
  int64_t bored_yield_duration_;

  std::shared_ptr<Configure> configure_;

  std::shared_ptr<core::Repository> repo_;

  std::shared_ptr<core::Repository> flow_repo_;

  std::shared_ptr<core::ContentRepository> content_repo_;
  // thread pool for components.
  utils::ThreadPool<utils::ComplexTaskResult> thread_pool_;
  // controller service provider reference
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider_;

 private:
  struct SchedulingInfo {
    std::chrono::time_point<std::chrono::steady_clock> start_time_ = std::chrono::steady_clock::now();
    // Mutable is required to be able to modify this while leaving in std::set
    mutable std::chrono::time_point<std::chrono::steady_clock> last_alert_time_ = std::chrono::steady_clock::now();
    std::string name_;
    std::string uuid_;

    explicit SchedulingInfo(const std::shared_ptr<core::Processor> &processor) :
      name_(processor->getName()),
      uuid_(processor->getUUIDStr()) {}

    bool operator <(const SchedulingInfo& o) const {
      return std::tie(start_time_, name_, uuid_) < std::tie(o.start_time_, o.name_, o.uuid_);
    }
  };

  // Logger
  std::shared_ptr<logging::Logger> logger_;
  mutable std::mutex watchdog_mtx_;  // used to protect the set below
  std::set<SchedulingInfo> scheduled_processors_;  // set was chosen to avoid iterator invalidation
  std::unique_ptr<utils::CallBackTimer> watchDogTimer_;
  std::chrono::milliseconds alert_time_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
