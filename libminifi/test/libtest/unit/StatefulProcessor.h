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

#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "core/Processor.h"
#include "core/StateManager.h"

namespace org::apache::nifi::minifi::processors {

class StatefulProcessor : public core::ProcessorImpl {
 public:
  using core::ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "A processor with state for test purposes.";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

  using HookType = std::function<void(core::StateManager&)>;
  void setHooks(HookType onScheduleHook, std::vector<HookType> onTriggerHooks);
  [[nodiscard]] bool hasFinishedHooks() const;

 private:
  mutable std::mutex mutex_;
  core::StateManager* state_manager_;
  HookType on_schedule_hook_;
  std::vector<HookType> on_trigger_hooks_;
  size_t on_trigger_hook_index_ = 0;
};

}  // namespace org::apache::nifi::minifi::processors
