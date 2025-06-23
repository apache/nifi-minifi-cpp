/**
 * @file TimerDrivenSchedulingAgent.cpp
 * TimerDrivenSchedulingAgent class implementation
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
#include "TimerDrivenSchedulingAgent.h"
#include <chrono>
#include <memory>

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

utils::TaskRescheduleInfo TimerDrivenSchedulingAgent::run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                         const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (running_ && processor->isRunning()) {
    const auto trigger_start_time = std::chrono::steady_clock::now();
    auto result = triggerAndCommit(processor, processContext, sessionFactory);
    if (!result) {
      logger_->log_warn("Trigger and commit failed for processor {}", processor->getName());
    }

    const auto next_scheduled_run = trigger_start_time + processor->getSchedulingPeriod();
    const auto yield_expiration_time = processor->getYieldExpirationTime();
    return utils::TaskRescheduleInfo::RetryAfter(std::max(next_scheduled_run, yield_expiration_time));
  }
  return utils::TaskRescheduleInfo::Done();
}

}  // namespace org::apache::nifi::minifi
