// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <map>
#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::core::controller {

/**
 * Controller Service Lookup pure virtual class.
 *
 * Purpose: Provide a mechanism that controllers can lookup information about
 * controller services.
 *
 */
class ControllerServiceLookup {
 public:
  ControllerServiceLookup() = default;

  virtual ~ControllerServiceLookup() = default;

  /**
   * Gets the controller service via the provided identifier. This overload returns the controller service in a global scope from all
   * available controller services in the flow.
   * @param identifier reference string for controller service.
   * @return controller service reference.
   */
  virtual std::shared_ptr<ControllerService> getControllerService(const std::string &identifier) const = 0;

  /**
   * Gets the controller service in the scope of the processor via the provided identifier.
   * @param identifier reference string for controller service.
   * @param processor_uuid uuid of the processor
   * @return controller service reference.
   */
  virtual std::shared_ptr<ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
