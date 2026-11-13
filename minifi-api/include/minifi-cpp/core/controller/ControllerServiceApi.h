// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <vector>

#include "ControllerServiceHandle.h"
#include "ControllerServiceContext.h"
#include "ControllerServiceDescriptor.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceApi {
 public:
  virtual ~ControllerServiceApi() = default;

  [[nodiscard]] virtual bool supportsDynamicProperties() const = 0;

  // Called after construction to advertise supported properties.
  virtual void initialize(ControllerServiceDescriptor& descriptor) = 0;
  // Called before being used from processors and other controller services depending on this one
  // the services in linked_services are already enabled
  // after this the controller service should be ready to be used.
  virtual void onEnable(ControllerServiceContext& context, const std::shared_ptr<Configure>& configuration, const std::vector<std::shared_ptr<ControllerServiceHandle>>& linked_services) = 0;
  // A handle that provides the actual service-specific functionality (will be dynamic_cast-ed to the expected interface)
  // it should be valid between onEnable and notifyStop.
  [[nodiscard]] virtual ControllerServiceHandle* getControllerServiceHandle() = 0;
  // In this method the service should release all resources, after this either onEnable is called again, or the service is destroyed.
  virtual void notifyStop() = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller
