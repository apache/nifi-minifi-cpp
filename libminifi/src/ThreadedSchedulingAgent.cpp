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

void ThreadedSchedulingAgent::schedule(
    std::shared_ptr<core::Processor> processor) {
  std::lock_guard<std::mutex> lock(mutex_);

  admin_yield_duration_ = 0;
  std::string yieldValue;

  if (configure_->get(Configure::nifi_administrative_yield_duration,
                      yieldValue)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(yieldValue, admin_yield_duration_, unit)
        && core::Property::ConvertTimeUnitToMS(admin_yield_duration_, unit,
                                               admin_yield_duration_)) {
      logger_->log_debug("nifi_administrative_yield_duration: [%d] ms",
                         admin_yield_duration_);
    }
  }

  bored_yield_duration_ = 0;
  if (configure_->get(Configure::nifi_bored_yield_duration, yieldValue)) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(yieldValue, bored_yield_duration_, unit)
        && core::Property::ConvertTimeUnitToMS(bored_yield_duration_, unit,
                                               bored_yield_duration_)) {
      logger_->log_debug("nifi_bored_yield_duration: [%d] ms",
                         bored_yield_duration_);
    }
  }

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_info(
        "Can not schedule threads for processor %s because it is not running",
        processor->getName().c_str());
    return;
  }

  std::map<std::string, std::vector<std::thread *>>::iterator it =
      _threads.find(processor->getUUIDStr());
  if (it != _threads.end()) {
    logger_->log_info(
        "Can not schedule threads for processor %s because there are existing threads running");
    return;
  }

  core::ProcessorNode processor_node(processor);
  auto processContext = std::make_shared<core::ProcessContext>(
      processor_node, controller_service_provider_, repo_);
  auto sessionFactory = std::make_shared<core::ProcessSessionFactory>(
      processContext.get());

  processor->onSchedule(processContext.get(), sessionFactory.get());

  std::vector<std::thread *> threads;
  for (int i = 0; i < processor->getMaxConcurrentTasks(); i++) {
    ThreadedSchedulingAgent *agent = this;
    std::thread *thread = new std::thread(
        [agent, processor, processContext, sessionFactory] () {
          agent->run(processor, processContext.get(), sessionFactory.get());
        });
    thread->detach();
    threads.push_back(thread);
    logger_->log_info("Scheduled thread %d running for process %s",
                      thread->get_id(), processor->getName().c_str());
  }
  _threads[processor->getUUIDStr().c_str()] = threads;

  return;
}

void ThreadedSchedulingAgent::unschedule(
    std::shared_ptr<core::Processor> processor) {
  std::lock_guard<std::mutex> lock(mutex_);
  logger_->log_info("Shutting down threads for processor %s/%s",
                    processor->getName().c_str(),
                    processor->getUUIDStr().c_str());

  if (processor->getScheduledState() != core::RUNNING) {
    logger_->log_info(
        "Cannot unschedule threads for processor %s because it is not running",
        processor->getName().c_str());
    return;
  }

  std::map<std::string, std::vector<std::thread *>>::iterator it =
      _threads.find(processor->getUUIDStr());

  if (it == _threads.end()) {
    logger_->log_info(
        "Cannot unschedule threads for processor %s because there are no existing threads running",
        processor->getName().c_str());
    return;
  }
  for (std::vector<std::thread *>::iterator itThread = it->second.begin();
      itThread != it->second.end(); ++itThread) {
    std::thread *thread = *itThread;
    logger_->log_info("Scheduled thread %d deleted for process %s",
                      thread->get_id(), processor->getName().c_str());
    delete thread;
  }
  _threads.erase(processor->getUUIDStr());
  processor->clearActiveTask();
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
