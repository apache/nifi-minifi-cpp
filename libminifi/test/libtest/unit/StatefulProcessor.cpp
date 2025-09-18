/**
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
#include "StatefulProcessor.h"

#include <string>

#include "minifi-cpp/Exception.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void StatefulProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  std::lock_guard<std::mutex> lock(mutex_);
  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  if (on_schedule_hook_) {
    on_schedule_hook_(*state_manager_);
  }
}

void StatefulProcessor::onTrigger(core::ProcessContext&, core::ProcessSession&) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (on_trigger_hook_index_ < on_trigger_hooks_.size()) {
    on_trigger_hooks_[on_trigger_hook_index_++](*state_manager_);
  }
}

void StatefulProcessor::setHooks(HookType onScheduleHook, std::vector<HookType> onTriggerHooks) {
  std::lock_guard<std::mutex> lock(mutex_);
  on_schedule_hook_ = std::move(onScheduleHook);
  on_trigger_hooks_ = std::move(onTriggerHooks);
}

bool StatefulProcessor::hasFinishedHooks() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return on_trigger_hook_index_ == on_trigger_hooks_.size();
}

REGISTER_RESOURCE(StatefulProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
