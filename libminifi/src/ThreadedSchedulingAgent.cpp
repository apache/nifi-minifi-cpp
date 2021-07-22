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

#include <cinttypes>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/ClassLoader.h"
#include "core/Connectable.h"
#include "core/ProcessorNode.h"
#include "core/ProcessContext.h"
#include "core/ProcessContextBuilder.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "utils/ValueParser.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void ThreadedSchedulingAgent::schedule(std::shared_ptr<core::Processor> processor) {
  std::lock_guard<std::mutex> lock(mutex_);

  admin_yield_duration_ = 100;  // We should prevent burning CPU in case of rollbacks
  std::string yieldValue;

  if (configure_->get(Configure::nifi_administrative_yield_duration, yieldValue)) {
    std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yieldValue);
    if (value) {
      admin_yield_duration_ = value->getMilliseconds();
      logger_->log_debug("nifi_administrative_yield_duration: [%" PRId64 "] ms", admin_yield_duration_);
    }
  }

  bored_yield_duration_ = 0;
  if (configure_->get(Configure::nifi_bored_yield_duration, yieldValue)) {
    std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yieldValue);
    if (value) {
      bored_yield_duration_ = value->getMilliseconds();
      logger_->log_debug("nifi_bored_yield_duration: [%" PRId64 "] ms", bored_yield_duration_);
    }
  }

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_debug("Can not schedule threads for processor %s because it is not running", processor->getName());
    return;
  }

  if (thread_pool_.isTaskRunning(processor->getUUIDStr())) {
    logger_->log_warn("Can not schedule threads for processor %s because there are existing threads running", processor->getName());
    return;
  }

  std::shared_ptr<core::ProcessorNode> processor_node = std::make_shared<core::ProcessorNode>(processor);

  auto contextBuilder = core::ClassLoader::getDefaultClassLoader().instantiate<core::ProcessContextBuilder>("ProcessContextBuilder", "ProcessContextBuilder");

  contextBuilder = contextBuilder->withContentRepository(content_repo_)->withFlowFileRepository(flow_repo_)->withProvider(controller_service_provider_)->withProvenanceRepository(repo_)
      ->withConfiguration(configure_);

  auto processContext = contextBuilder->build(processor_node);

  auto sessionFactory = std::make_shared<core::ProcessSessionFactory>(processContext);

  processor->onSchedule(processContext, sessionFactory);

  std::vector<std::thread *> threads;

  ThreadedSchedulingAgent *agent = this;
  for (int i = 0; i < processor->getMaxConcurrentTasks(); i++) {
    // reference the disable function from serviceNode
    processor->incrementActiveTasks();

    std::function<utils::TaskRescheduleInfo()> f_ex = [agent, processor, processContext, sessionFactory] () {
      return agent->run(processor, processContext, sessionFactory);
    };

    // create a functor that will be submitted to the thread pool.
    utils::Worker<utils::TaskRescheduleInfo> functor(f_ex, processor->getUUIDStr(), std::make_unique<utils::ComplexMonitor>());
    // move the functor into the thread pool. While a future is returned
    // we aren't terribly concerned with the result.
    std::future<utils::TaskRescheduleInfo> future;
    thread_pool_.execute(std::move(functor), future);
  }
  logger_->log_debug("Scheduled thread %d concurrent workers for for process %s", processor->getMaxConcurrentTasks(), processor->getName());
  processors_running_.insert(processor->getUUID());
}

void ThreadedSchedulingAgent::stop() {
  SchedulingAgent::stop();
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& processor_id : processors_running_) {
    logger_->log_error("SchedulingAgent is stopped before processor was unscheduled: %s", processor_id.to_string());
    thread_pool_.stopTasks(processor_id.to_string());
  }
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

  processors_running_.erase(processor->getUUID());
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
