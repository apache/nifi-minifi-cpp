// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/controller/ControllerServiceProvider.h"

#include <memory>
#include <string>

#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core::controller {

std::shared_ptr<ControllerService> ControllerServiceProvider::getControllerService(const std::string &identifier) const {
  auto service = controller_map_->get(identifier);
  if (service != nullptr) {
    return service->getControllerServiceImplementation();
  } else {
    return nullptr;
  }
}

std::shared_ptr<ControllerService> ControllerServiceProvider::getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const {
  auto service = controller_map_->get(identifier, processor_uuid);
  if (service != nullptr) {
    return service->getControllerServiceImplementation();
  } else {
    return nullptr;
  }
}

void ControllerServiceProvider::putControllerServiceNode(const std::string& primary_key,
    const std::shared_ptr<ControllerServiceNode>& controller_service_node,
    ProcessGroup* process_group,
    const std::optional<std::string>& alternative_key) {
  gsl_Expects(controller_map_);
  controller_map_->put(primary_key, controller_service_node, process_group);
  if (alternative_key) {
    controller_map_->registerAlternativeKey(primary_key, *alternative_key);
  }
}

}  // namespace org::apache::nifi::minifi::core::controller
