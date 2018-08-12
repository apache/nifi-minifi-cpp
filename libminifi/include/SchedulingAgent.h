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

#include <uuid/uuid.h>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

/**
 * Uses the wait time for a given worker to determine if it is eligible to run
 */
class TimerAwareMonitor : public utils::AfterExecute<uint64_t> {
 public:
  TimerAwareMonitor(std::atomic<bool> *run_monitor)
      : current_wait_(0),
        run_monitor_(run_monitor) {
  }
  explicit TimerAwareMonitor(TimerAwareMonitor &&other)
      : AfterExecute(std::move(other)),
        run_monitor_(std::move(other.run_monitor_)) {
    current_wait_.store(other.current_wait_.load());
  }
  virtual bool isFinished(const uint64_t &result) {
    current_wait_.store(result);
    if (*run_monitor_) {
      return false;
    }
    return true;
  }
  virtual bool isCancelled(const uint64_t &result) {
    if (*run_monitor_) {
      return false;
    }
    return true;
  }
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual int64_t wait_time() {
    return current_wait_.load();
  }
 protected:

  std::atomic<uint64_t> current_wait_;
  std::atomic<bool> *run_monitor_;
};

class SingleRunMonitor : public TimerAwareMonitor {
 public:
  SingleRunMonitor(std::atomic<bool> *run_monitor)
      : TimerAwareMonitor(run_monitor) {
  }
  explicit SingleRunMonitor(TimerAwareMonitor &&other)
      : TimerAwareMonitor(std::move(other)) {
  }
  virtual bool isFinished(const uint64_t &result) {
    if (result == 0) {
      return true;
    } else {
      current_wait_.store(result);
      if (*run_monitor_) {
        return false;
      }
      return true;
    }
  }
};

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
        logger_(logging::LoggerFactory<SchedulingAgent>::getLogger()) {
    running_ = false;
    repo_ = repo;
    flow_repo_ = flow_repo;
    /**
     * To facilitate traces we cannot use daemon threads -- this could potentially cause blocking on I/O; however, it's a better path
     * to be able to debug why an agent doesn't work and still allow a restart via updates in these cases.
     */
    auto csThreads = configure_->getInt(Configure::nifi_flow_engine_threads, 2);
    auto pool = utils::ThreadPool<uint64_t>(csThreads, false, controller_service_provider, "SchedulingAgent");
    thread_pool_ = std::move(pool);
    thread_pool_.start();
  }
  // Destructor
  virtual ~SchedulingAgent() {

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

 public:
  virtual std::future<uint64_t> enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);
  virtual std::future<uint64_t> disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);
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
  utils::ThreadPool<uint64_t> thread_pool_;
  // controller service provider reference
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider_;

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
