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
#include "core/ProcessSessionFactory.h"
#include "utils/ValueParser.h"

using namespace std::literals::chrono_literals;


namespace org::apache::nifi::minifi {

void ThreadedSchedulingAgent::schedule(core::Processor* processor) {
  std::lock_guard<std::mutex> lock(mutex_);

  admin_yield_duration_ = 100ms;  // We should prevent burning CPU in case of rollbacks
  std::string yieldValue;

  if (configure_->get(Configure::nifi_administrative_yield_duration, yieldValue)) {
    std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yieldValue);
    if (value) {
      admin_yield_duration_ = value->getMilliseconds();
      logger_->log_debug("nifi_administrative_yield_duration: [{}]", admin_yield_duration_);
    }
  }

  bored_yield_duration_ = 0ms;
  if (configure_->get(Configure::nifi_bored_yield_duration, yieldValue)) {
    std::optional<core::TimePeriodValue> value = core::TimePeriodValue::fromString(yieldValue);
    if (value) {
      bored_yield_duration_ = value->getMilliseconds();
      logger_->log_debug("nifi_bored_yield_duration: [{}]", bored_yield_duration_);
    }
  }

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_debug("Can not schedule threads for processor {} because it is not running", processor->getName());
    return;
  }

  if (thread_pool_.isTaskRunning(processor->getUUIDStr())) {
    logger_->log_warn("Can not schedule threads for processor {} because there are existing threads running", processor->getName());
    return;
  }

  std::shared_ptr<core::ProcessorNode> processor_node = std::make_shared<core::ProcessorNodeImpl>(processor);

  std::shared_ptr<core::ProcessContextBuilder> contextBuilder = core::ClassLoader::getDefaultClassLoader().instantiate<core::ProcessContextBuilder>("ProcessContextBuilder", "ProcessContextBuilder");

  contextBuilder = contextBuilder->withContentRepository(content_repo_)->withFlowFileRepository(flow_repo_)->withProvider(controller_service_provider_)->withProvenanceRepository(repo_)
      ->withConfiguration(configure_);

  auto process_context = contextBuilder->build(processor_node);

  auto session_factory = std::make_shared<core::ProcessSessionFactoryImpl>(process_context);

  processor->onSchedule(*process_context, *session_factory);

  std::vector<std::thread *> threads;

  ThreadedSchedulingAgent *agent = this;
  for (int i = 0; i < processor->getMaxConcurrentTasks(); i++) {
    // reference the disable function from serviceNode
    processor->incrementActiveTasks();

    std::function<utils::TaskRescheduleInfo()> f_ex = [agent, processor, process_context, session_factory] () {
      return agent->run(processor, process_context, session_factory);
    };

    std::future<utils::TaskRescheduleInfo> future;
    thread_pool_.execute(utils::Worker{f_ex, processor->getUUIDStr()}, future);
  }
  logger_->log_debug("Scheduled thread {} concurrent workers for for process {}", processor->getMaxConcurrentTasks(), processor->getName());
  processors_running_.insert(processor->getUUID());
}

void ThreadedSchedulingAgent::stop() {
  SchedulingAgent::stop();
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& processor_id : processors_running_) {
    logger_->log_error("SchedulingAgent is stopped before processor was unscheduled: {}", processor_id.to_string());
    thread_pool_.stopTasks(processor_id.to_string());
  }
}

void ThreadedSchedulingAgent::unschedule(core::Processor* processor) {
  std::lock_guard<std::mutex> lock(mutex_);
  logger_->log_debug("Shutting down threads for processor {}/{}", processor->getName(), processor->getUUIDStr());

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_warn("Cannot unschedule threads for processor {} because it is not running", processor->getName());
    return;
  }

  thread_pool_.stopTasks(processor->getUUIDStr());

  processor->clearActiveTask();

  processor->setScheduledState(core::STOPPED);

  processors_running_.erase(processor->getUUID());
}

}  // namespace org::apache::nifi::minifi
