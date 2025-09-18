/**
 * @file SchedulingAgent.cpp
 * SchedulingAgent class implementation
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
#include "SchedulingAgent.h"

#include <chrono>
#include <memory>
#include <thread>
#include <utility>

#include "core/Processor.h"
#include "minifi-cpp/utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace {
bool hasWorkToDo(org::apache::nifi::minifi::core::Processor* processor) {
  // Whether it has work to do
  return processor->getTriggerWhenEmpty() || !processor->hasIncomingConnections() || processor->isWorkAvailable();
}
}  // namespace

namespace org::apache::nifi::minifi {

bool SchedulingAgent::processorYields(core::Processor* processor) const {
  if (processor->isYield()) {
    logger_->log_debug("Not running {} since it must yield", processor->getName());
    return true;
  }

  // No need to yield, reset yield expiration to 0
  processor->clearYield();

  auto bored_yield_duration = bored_yield_duration_ > 0ms ? bored_yield_duration_ : 10ms;

  if (!hasWorkToDo(processor)) {
    processor->yield(bored_yield_duration);
    return true;
  }
  if (processor->isThrottledByBackpressure()) {
    logger_->log_debug("backpressure applied because too much outgoing for {} {}", processor->getUUIDStr(), processor->getName());
    processor->yield(bored_yield_duration);
    return true;
  }

  return false;
}

nonstd::expected<void, std::exception_ptr> SchedulingAgent::triggerAndCommit(core::Processor* processor,
    const std::shared_ptr<core::ProcessContext>& process_context,
    const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  gsl_Expects(processor);
  if (processorYields(processor)) {
    return {};
  }

  auto processor_scheduling_info = SchedulingInfo(processor);
  {
    std::lock_guard<std::mutex> lock(watchdog_mtx_);
    scheduled_processors_.push_back(gsl::make_not_null(&processor_scheduling_info));
  }

  const auto guard = gsl::finally([this, &processor_scheduling_info](){
    std::lock_guard<std::mutex> lock(watchdog_mtx_);
    [[maybe_unused]] const auto erased_scheduling_infos_count = std::erase(scheduled_processors_, gsl::make_not_null(&processor_scheduling_info));
    gsl_Assert(1 == erased_scheduling_infos_count);
  });


  processor->incrementActiveTasks();
  auto decrement_task = gsl::finally([processor]() { processor->decrementActiveTask(); });

  try {
    processor->triggerAndCommit(process_context, session_factory);
  } catch (const std::exception& exception) {
    logger_->log_warn("Caught Exception during SchedulingAgent::onTrigger of processor {} (uuid: {}), type: {}, what: {}",
        processor->getName(), processor->getUUIDStr(), typeid(exception).name(), exception.what());
    processor->yield(admin_yield_duration_);
    return nonstd::make_unexpected(std::current_exception());
  } catch (...) {
    logger_->log_warn("Caught Exception during SchedulingAgent::onTrigger of processor {} (uuid: {}), type: {}",
        processor->getName(), processor->getUUIDStr(), getCurrentExceptionTypeName());
    processor->yield(admin_yield_duration_);
    return nonstd::make_unexpected(std::current_exception());
  }
  return {};
}

nonstd::expected<bool, std::exception_ptr> SchedulingAgent::trigger(core::Processor* processor,
    const std::shared_ptr<core::ProcessContext>& process_context,
    const std::shared_ptr<core::ProcessSession>& process_session) {
  gsl_Expects(processor);
  if (processorYields(processor)) {
    return false;
  }

  auto processor_scheduling_info = SchedulingInfo(processor);
  {
    std::lock_guard<std::mutex> lock(watchdog_mtx_);
    scheduled_processors_.push_back(gsl::make_not_null(&processor_scheduling_info));
  }

  const auto guard = gsl::finally([this, &processor_scheduling_info](){
    std::lock_guard<std::mutex> lock(watchdog_mtx_);
    [[maybe_unused]] const auto erased_scheduling_infos_count = std::erase(scheduled_processors_, gsl::make_not_null(&processor_scheduling_info));
    gsl_Assert(1 == erased_scheduling_infos_count);
  });

  processor->incrementActiveTasks();
  auto decrement_task = gsl::finally([processor]() { processor->decrementActiveTask(); });
  try {
    processor->trigger(process_context, process_session);
  } catch (const std::exception& exception) {
    logger_->log_warn("Caught Exception during SchedulingAgent::onTrigger of processor {} (uuid: {}), type: {}, what: {}",
        processor->getName(), processor->getUUIDStr(), typeid(exception).name(), exception.what());
    processor->yield(admin_yield_duration_);
    return nonstd::make_unexpected(std::current_exception());
  } catch (...) {
    logger_->log_warn("Caught Exception during SchedulingAgent::onTrigger of processor {} (uuid: {}), type: {}",
        processor->getName(), processor->getUUIDStr(), getCurrentExceptionTypeName());
    processor->yield(admin_yield_duration_);
    return nonstd::make_unexpected(std::current_exception());
  }
  return true;
}

void SchedulingAgent::watchDogFunc() {
  std::lock_guard<std::mutex> lock(watchdog_mtx_);
  auto now = std::chrono::steady_clock::now();
  for (const auto& info : scheduled_processors_) {
    auto elapsed = now - info->last_alert_time_;
    if (elapsed > alert_time_) {
      int64_t elapsed_ms{ std::chrono::duration_cast<std::chrono::milliseconds>(now - info->start_time_).count() };
      logger_->log_warn("{}::onTrigger has been running for {}  ms in {}", info->name_, elapsed_ms, info->uuid_);
      info->last_alert_time_ = now;
    }
  }
}

}  // namespace org::apache::nifi::minifi
