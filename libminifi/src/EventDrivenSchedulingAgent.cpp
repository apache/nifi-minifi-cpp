/**
 * @file EventDrivenSchedulingAgent.cpp
 * EventDrivenSchedulingAgent class implementation
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
#include "EventDrivenSchedulingAgent.h"
#include <chrono>
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "minifi-cpp/core/Property.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

void EventDrivenSchedulingAgent::schedule(core::Processor* processor) {
  if (!processor->hasIncomingConnections()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "EventDrivenSchedulingAgent cannot schedule processor without incoming connection!");
  }
  ThreadedSchedulingAgent::schedule(processor);
}

utils::TaskRescheduleInfo EventDrivenSchedulingAgent::run(core::Processor* processor,
    const std::shared_ptr<core::ProcessContext>& process_context,
    const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  if (!this->running_) {
    return utils::TaskRescheduleInfo::Done();
  }
  if (processorYields(processor)) {
    return utils::TaskRescheduleInfo::RetryAfter(processor->getYieldExpirationTime());
  }

  const auto start_time = std::chrono::steady_clock::now();
  // trigger processor while it has work to do, but no more than the configured nifi.flow.engine.event.driven.time.slice

  const auto process_session = session_factory->createSession();
  process_session->setMetrics(processor->getMetrics());
  bool needs_commit = true;

  while (processor->isRunning() && (std::chrono::steady_clock::now() - start_time < time_slice_)) {
    const auto trigger_result = this->trigger(processor, process_context, process_session);
    if (!trigger_result) {
      try {
        std::rethrow_exception(trigger_result.error());
      } catch (const std::exception& exception) {
        logger_->log_warn("Caught \"{}\" ({}) during Processor::onTrigger of processor: {} ({})",
            exception.what(), typeid(exception).name(), processor->getUUIDStr(), processor->getName());
        needs_commit = false;
        break;
      } catch (...) {
        logger_->log_warn("Caught unknown exception during Processor::onTrigger of processor: {} ({})", processor->getUUIDStr(), processor->getName());
        needs_commit = false;
        break;
      }
    }
    if (!*trigger_result) {
      logger_->log_trace("Processor {} ({}) yielded", processor->getUUIDStr(), processor->getName());
      break;
    }
  }
  if (needs_commit) {
    try {
      process_session->commit();
    } catch (const std::exception& exception) {
      logger_->log_warn("Caught \"{}\" ({}) during ProcessSession::commit after triggering processor: {} ({})",
      exception.what(), typeid(exception).name(), processor->getUUIDStr(), processor->getName());
      auto rollback_result = process_session->rollbackNoThrow();
      if (!rollback_result) {
        logger_->log_warn("Rollback after commit failure failed with: {}", rollback_result.error());
      }
    } catch (...) {
      logger_->log_warn("Caught unknown exception during ProcessSession::commit after triggering processor: {} ({})", processor->getUUIDStr(), processor->getName());
      auto rollback_result = process_session->rollbackNoThrow();
      if (!rollback_result) {
        logger_->log_warn("Rollback after commit failure failed with: {}", rollback_result.error());
      }
    }
  } else {
    auto rollback_result = process_session->rollbackNoThrow();
    if (!rollback_result) {
      logger_->log_warn("Rollback after triggering processor failed with: {}", rollback_result.error());
    }
  }

  if (processor->isYield()) {
    return utils::TaskRescheduleInfo::RetryAfter(processor->getYieldExpirationTime());
  }

  return utils::TaskRescheduleInfo::RetryImmediately();  // Let's continue work as soon as a thread is available
}

}  // namespace org::apache::nifi::minifi
