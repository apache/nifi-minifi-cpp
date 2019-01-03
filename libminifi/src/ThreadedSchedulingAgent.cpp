/**
 * @file ThreadedSchedulingAgent.cpp
 * ThreadedSchedulingAgent class implementation
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
#include "ThreadedSchedulingAgent.h"
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <thread>
#include <iostream>
#include "core/Connectable.h"
#include "core/ProcessorNode.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void ThreadedSchedulingAgent::schedule(std::shared_ptr<core::Processor> processor) {
  std::lock_guard<std::mutex> lock(mutex_);

  admin_yield_duration_ = 0;
  std::string yieldValue;

  if (configure_->get(Configure::nifi_administrative_yield_duration, yieldValue)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(yieldValue, admin_yield_duration_, unit) && core::Property::ConvertTimeUnitToMS(admin_yield_duration_, unit, admin_yield_duration_)) {
      logger_->log_debug("nifi_administrative_yield_duration: [%ll] ms", admin_yield_duration_);
    }
  }

  bored_yield_duration_ = 0;
  if (configure_->get(Configure::nifi_bored_yield_duration, yieldValue)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(yieldValue, bored_yield_duration_, unit) && core::Property::ConvertTimeUnitToMS(bored_yield_duration_, unit, bored_yield_duration_)) {
      logger_->log_debug("nifi_bored_yield_duration: [%ll] ms", bored_yield_duration_);
    }
  }

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_debug("Can not schedule threads for processor %s because it is not running", processor->getName());
    return;
  }

  if (thread_pool_.isRunning(processor->getUUIDStr())) {
    logger_->log_warn("Can not schedule threads for processor %s because there are existing threads running");
    return;
  }

  std::shared_ptr<core::ProcessorNode> processor_node = std::make_shared<core::ProcessorNode>(processor);

  auto processContext = std::make_shared<core::ProcessContext>(processor_node, controller_service_provider_, repo_, flow_repo_, configure_, content_repo_);
  auto sessionFactory = std::make_shared<core::ProcessSessionFactory>(processContext);

  processor->onSchedule(processContext, sessionFactory);

  std::vector<std::thread *> threads;

  ThreadedSchedulingAgent *agent = this;
  for (int i = 0; i < processor->getMaxConcurrentTasks(); i++) {
    // reference the disable function from serviceNode
    processor->incrementActiveTasks();

    std::function<uint64_t()> f_ex = [agent, processor, processContext, sessionFactory] () {
      return agent->run(processor, processContext, sessionFactory);
    };

    // create a functor that will be submitted to the thread pool.
    std::unique_ptr<TimerAwareMonitor> monitor = std::unique_ptr<TimerAwareMonitor>(new TimerAwareMonitor(&running_));
    utils::Worker<uint64_t> functor(f_ex, processor->getUUIDStr(), std::move(monitor));
    // move the functor into the thread pool. While a future is returned
    // we aren't terribly concerned with the result.
    std::future<uint64_t> future;
    thread_pool_.execute(std::move(functor), future);
  }
  logger_->log_debug("Scheduled thread %d concurrent workers for for process %s", processor->getMaxConcurrentTasks(), processor->getName());
  return;
}

void ThreadedSchedulingAgent::stop() {
  SchedulingAgent::stop();
  thread_pool_.shutdown();
}

void ThreadedSchedulingAgent::unschedule(std::shared_ptr<core::Processor> processor) {
  std::lock_guard<std::mutex> lock(mutex_);
  logger_->log_debug("Shutting down threads for processor %s/%s", processor->getName(), processor->getUUIDStr());

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_warn("Cannot unschedule threads for processor %s because it is not running", processor->getName());
    return;
  }

  thread_pool_.stopTasks(processor->getUUIDStr());

  processor->clearActiveTask();

  processor->setScheduledState(core::STOPPED);
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
