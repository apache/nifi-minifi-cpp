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

#include <string>

#include "StatefulProcessor.h"
#include "Exception.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

void StatefulProcessor::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);
  std::lock_guard<std::mutex> lock(mutex_);
  stateManager_ = context->getStateManager();
  if (stateManager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  if (onScheduleHook_) {
    onScheduleHook_(*stateManager_);
  }
}

void StatefulProcessor::onTrigger(const std::shared_ptr<core::ProcessContext>&, const std::shared_ptr<core::ProcessSession>&) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (onTriggerHookIndex_ < onTriggerHooks_.size()) {
    onTriggerHooks_[onTriggerHookIndex_++](*stateManager_);
  }
}

void StatefulProcessor::setHooks(HookType onScheduleHook, std::vector<HookType> onTriggerHooks) {
  std::lock_guard<std::mutex> lock(mutex_);
  onScheduleHook_ = std::move(onScheduleHook);
  onTriggerHooks_ = std::move(onTriggerHooks);
}

bool StatefulProcessor::hasFinishedHooks() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return onTriggerHookIndex_ == onTriggerHooks_.size();
}

REGISTER_RESOURCE(StatefulProcessor, "A processor with state for test purposes.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
