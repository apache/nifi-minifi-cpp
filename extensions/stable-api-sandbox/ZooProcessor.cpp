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

#include "ZooProcessor.h"

#include "AnimalControllerServices.h"
#include "api/core/ProcessContext.h"

namespace org::apache::nifi::minifi::api_sandbox {

MinifiStatus ZooProcessor::onTriggerImpl(api::core::ProcessContext& process_context, api::core::ProcessSession& process_session) {
  if (const auto can_fly_opaque = process_context.getControllerService(CanFlyService)) {
    if (*can_fly_opaque) {
      const auto can_fly_controller_name = process_context.getProperty(CanFlyService, nullptr) | utils::orThrow("Should be here");
      const CanFlyControllerApi* can_fly = reinterpret_cast<CanFlyControllerApi*>(*can_fly_opaque);
      logger_->log_critical("Can {} fly? {}", can_fly_controller_name, can_fly->canFly());
    }
  }
  if (const auto num_of_legs_opaque = process_context.getControllerService(NumberOfLegsService)) {
    if (*num_of_legs_opaque) {
      const auto num_of_legs_name = process_context.getProperty(NumberOfLegsService, nullptr) | utils::orThrow("Should be here");
      const NumberOfLegsControllerApi* num_legs = reinterpret_cast<NumberOfLegsControllerApi*>(*num_of_legs_opaque);
      logger_->log_critical("{} has {} legs", num_of_legs_name, num_legs->numberOfLegs());
    }
  }
  return ProcessorImpl::onTriggerImpl(process_context, process_session);
}

MinifiStatus ZooProcessor::onScheduleImpl(api::core::ProcessContext& process_context) {
  return ProcessorImpl::onScheduleImpl(process_context);
}
}  // namespace org::apache::nifi::minifi::api_sandbox
