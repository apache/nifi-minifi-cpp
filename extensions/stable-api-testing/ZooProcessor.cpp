// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "ZooProcessor.h"

#include "AnimalControllerServices.h"
#include "api/core/ProcessContext.h"

namespace org::apache::nifi::minifi::api_testing {

minifi_status ZooProcessor::onTriggerImpl(api::core::ProcessContext& process_context, api::core::ProcessSession& process_session) {
  if (const auto can_fly_opaque = process_context.getControllerService(CanFlyService)) {
    if (*can_fly_opaque) {
      const auto can_fly_controller_name = process_context.getProperty(CanFlyService, nullptr) | utils::orThrow("Should be here");
      const CanFlyControllerApi* const can_fly = reinterpret_cast<CanFlyControllerApi*>(*can_fly_opaque);
      logger_->log_critical("Can {} fly? {}", can_fly_controller_name, can_fly->canFly());
    }
  }
  if (const auto num_of_legs_opaque = process_context.getControllerService(NumberOfLegsService)) {
    if (*num_of_legs_opaque) {
      const auto num_of_legs_name = process_context.getProperty(NumberOfLegsService, nullptr) | utils::orThrow("Should be here");
      const NumberOfLegsControllerApi* const num_legs = reinterpret_cast<NumberOfLegsControllerApi*>(*num_of_legs_opaque);
      logger_->log_critical("{} has {} legs", num_of_legs_name, num_legs->numberOfLegs());
    }
  }
  if (auto ssl_data = process_context.getSslData(SSLContextService)) {
    logger_->log_critical("Has ssl_data? {}", ssl_data->has_value());
  }

  return ProcessorImpl::onTriggerImpl(process_context, process_session);
}

minifi_status ZooProcessor::onScheduleImpl(api::core::ProcessContext& process_context) {
  return ProcessorImpl::onScheduleImpl(process_context);
}
}  // namespace org::apache::nifi::minifi::api_testing
