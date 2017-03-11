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
#include <chrono>
#include <thread>
#include <iostream>
#include "TimerDrivenSchedulingAgent.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void TimerDrivenSchedulingAgent::run(
    std::shared_ptr<core::Processor> processor,
    core::ProcessContext *processContext,
    core::ProcessSessionFactory *sessionFactory) {
  while (this->running_) {
    bool shouldYield = this->onTrigger(processor, processContext,
                                       sessionFactory);

    if (processor->isYield()) {
      // Honor the yield
      std::this_thread::sleep_for(
          std::chrono::milliseconds(processor->getYieldTime()));
    } else if (shouldYield && this->_boredYieldDuration > 0) {
      // No work to do or need to apply back pressure
      std::this_thread::sleep_for(
          std::chrono::milliseconds(this->_boredYieldDuration));
    }
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(processor->getSchedulingPeriodNano()));
  }
  return;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
