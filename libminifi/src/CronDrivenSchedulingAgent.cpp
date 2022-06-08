/**
 * @file CronDrivenSchedulingAgent.cpp
 * CronDrivenSchedulingAgent class implementation
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
#include "CronDrivenSchedulingAgent.h"
#include <chrono>
#include <memory>
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"

namespace org::apache::nifi::minifi {

utils::TaskRescheduleInfo CronDrivenSchedulingAgent::run(core::Processor* processor,
                                                         const std::shared_ptr<core::ProcessContext>& processContext,
                                                         const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) {
  using namespace std::literals::chrono_literals;
  using std::chrono::ceil;
  using std::chrono::seconds;
  using std::chrono::milliseconds;
  using std::chrono::time_point_cast;
  using std::chrono::system_clock;

  if (this->running_ && processor->isRunning()) {
    auto uuid = processor->getUUID();
    auto current_time = date::make_zoned<seconds>(date::current_zone(), time_point_cast<seconds>(system_clock::now()));
    std::lock_guard<std::mutex> lock(mutex_);

    schedules_.emplace(uuid, utils::Cron(processor->getCronPeriod()));
    last_exec_.emplace(uuid, current_time.get_local_time());

    auto last_trigger = last_exec_[uuid];
    auto next_to_last_trigger = schedules_.at(uuid).calculateNextTrigger(last_trigger);
    if (!next_to_last_trigger)
      return utils::TaskRescheduleInfo::Done();

    if (*next_to_last_trigger > current_time.get_local_time())
      return utils::TaskRescheduleInfo::RetryIn(ceil<milliseconds>(*next_to_last_trigger-current_time.get_local_time()));

    last_exec_[uuid] = current_time.get_local_time();
    bool shouldYield = this->onTrigger(processor, processContext, sessionFactory);

    if (processor->isYield()) {
      return utils::TaskRescheduleInfo::RetryIn(processor->getYieldTime());
    } else if (shouldYield && this->bored_yield_duration_ > 0ms) {
      return utils::TaskRescheduleInfo::RetryIn(this->bored_yield_duration_);
    }

    auto next_trigger = schedules_.at(uuid).calculateNextTrigger(current_time.get_local_time());
    if (!next_trigger)
      return utils::TaskRescheduleInfo::Done();
    return utils::TaskRescheduleInfo::RetryIn(ceil<milliseconds>(*next_trigger-current_time.get_local_time()));
  }
  return utils::TaskRescheduleInfo::Done();
}

}  // namespace org::apache::nifi::minifi
