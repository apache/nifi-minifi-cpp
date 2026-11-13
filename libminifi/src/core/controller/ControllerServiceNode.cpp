// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/controller/ControllerServiceNode.h"
#include <memory>
#include <vector>

namespace org::apache::nifi::minifi::core::controller {

std::shared_ptr<ControllerService> ControllerServiceNode::getControllerServiceImplementation() const {
  return controller_service_;
}

const std::vector<ControllerServiceNode*>& ControllerServiceNode::getLinkedControllerServices() const {
  return linked_controller_services_;
}

}  // namespace org::apache::nifi::minifi::core::controller
