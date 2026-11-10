// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "AnimalControllerServices.h"

#include "api/utils/ProcessorConfigUtils.h"
namespace org::apache::nifi::minifi::api_testing {

minifi_status DogController::enableImpl(api::core::ControllerServiceContext& ctx) {
  this->has_jetpack_ = ctx.getProperty(HasJetpack) | minifi::utils::andThen(parsing::parseBool) |
      minifi::utils::orThrow(fmt::format("Expected parsable bool from \"{}\"", HasJetpack.name));

  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::api_testing
