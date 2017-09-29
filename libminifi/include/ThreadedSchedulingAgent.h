/**
 * @file ThreadedSchedulingAgent.h
 * ThreadedSchedulingAgent class declaration
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
#ifndef __THREADED_SCHEDULING_AGENT_H__
#define __THREADED_SCHEDULING_AGENT_H__

#include <chrono>
#include "properties/Configure.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Processor.h"
#include "core/Repository.h"
#include "core/ProcessContext.h"
#include "SchedulingAgent.h"

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
      : run_monitor_(run_monitor),
        current_wait_(0) {

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
 private:

  std::atomic<uint64_t> current_wait_;
  std::atomic<bool> *run_monitor_;
};

/**
 * An abstract scheduling agent which creates and manages a pool of threads for
 * each processor scheduled.
 */
class ThreadedSchedulingAgent : public SchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new threaded scheduling agent.
   */
  ThreadedSchedulingAgent(std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider, std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_repo,
                          std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration)
      : SchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configuration),
        logger_(logging::LoggerFactory<ThreadedSchedulingAgent>::getLogger()) {

    utils::ThreadPool<uint64_t> pool = utils::ThreadPool<uint64_t>(configure_->getInt(Configure::nifi_flow_engine_threads, 2), true);
    thread_pool_ = std::move(pool);
    thread_pool_.start();

  }
  // Destructor
  virtual ~ThreadedSchedulingAgent() {
  }

  // Run function for the thread
  virtual uint64_t run(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext,
                       const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) = 0;

 public:
  // schedule, overwritten by different DrivenTimerDrivenSchedulingAgent
  virtual void schedule(std::shared_ptr<core::Processor> processor);
  // unschedule, overwritten by different DrivenTimerDrivenSchedulingAgent
  virtual void unschedule(std::shared_ptr<core::Processor> processor);

  virtual void stop();
 protected:
  utils::ThreadPool<uint64_t> thread_pool_;

 protected:

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ThreadedSchedulingAgent(const ThreadedSchedulingAgent &parent);
  ThreadedSchedulingAgent &operator=(const ThreadedSchedulingAgent &parent);
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
