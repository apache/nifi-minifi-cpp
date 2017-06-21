/**
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

#include "core/state/ProcessorController.h"
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

ProcessorController::ProcessorController(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<SchedulingAgent> &scheduler)
    : processor_(processor),
      scheduler_(scheduler) {
}

ProcessorController::~ProcessorController() {
}
/**
 * Start the client
 */
int16_t ProcessorController::start() {
  processor_->setScheduledState(core::ScheduledState::RUNNING);
  scheduler_->schedule(processor_);
  return 0;
}
/**
 * Stop the client
 */
int16_t ProcessorController::stop(bool force, uint64_t timeToWait) {
  scheduler_->unschedule(processor_);
  return 0;
}

bool ProcessorController::isRunning() {
  return processor_->isRunning();
}

int16_t ProcessorController::pause() {
  scheduler_->unschedule(processor_);
  return 0;
}

} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
