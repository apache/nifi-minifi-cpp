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
#include <memory>
#include <thread>
#include <iostream>
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

uint64_t EventDrivenSchedulingAgent::run(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                         const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  while (this->running_) {
    bool shouldYield = this->onTrigger(processor, processContext, sessionFactory);

    if (processor->isYield()) {
      // Honor the yield
      return processor->getYieldTime();
    } else if (shouldYield && this->bored_yield_duration_ > 0) {
      // No work to do or need to apply back pressure
      return this->bored_yield_duration_;
    }

    // Block until work is available

    processor->waitForWork(1000);

    if (!processor->isWorkAvailable()) {
      return 1000;
    }
  }
  return 0;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
