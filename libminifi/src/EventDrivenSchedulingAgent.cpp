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
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"
#include "core/Property.h"

using namespace std::literals::chrono_literals;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void EventDrivenSchedulingAgent::schedule(core::Processor* processor) {
  if (!processor->hasIncomingConnections()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "EventDrivenSchedulingAgent cannot schedule processor without incoming connection!");
  }
  ThreadedSchedulingAgent::schedule(processor);
}

utils::TaskRescheduleInfo EventDrivenSchedulingAgent::run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                         const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (this->running_) {
    auto start_time = std::chrono::steady_clock::now();
    // trigger processor until it has work to do, but no more than half a sec
    while (processor->isRunning() && (std::chrono::steady_clock::now() - start_time < time_slice_)) {
      bool shouldYield = this->onTrigger(processor, processContext, sessionFactory);
      if (processor->isYield()) {
        // Honor the yield
        return utils::TaskRescheduleInfo::RetryIn(std::chrono::milliseconds(processor->getYieldTime()));
      } else if (shouldYield) {
        // No work to do or need to apply back pressure
        return utils::TaskRescheduleInfo::RetryIn(this->bored_yield_duration_ > 0ms ? this->bored_yield_duration_ : 10ms);  // No work left to do, stand by
      }
    }
    return utils::TaskRescheduleInfo::RetryImmediately();  // Let's continue work as soon as a thread is available
  }
  return utils::TaskRescheduleInfo::Done();
}

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
